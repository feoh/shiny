#include "ofApp.h"

#include <algorithm>
#include <cmath>

namespace {
// Hues in openFrameworks HSB color space are stored from 0 to 255.  Advancing
// consecutive waves by an irrational fraction related to the golden ratio spreads
// colors evenly around the hue wheel instead of clumping similar colors together.
constexpr float kGoldenHue = 0.61803398875f;

// Keep a hue value inside openFrameworks' 0-255 hue range.  std::fmod can return
// a negative remainder for negative inputs, so the second line wraps that back
// into the visible range.
float wrappedHue(float value) {
    value = std::fmod(value, 255.0f);
    return value < 0.0f ? value + 255.0f : value;
}
}

void ofApp::setup() {
    // Basic renderer/window defaults.  The app relies on alpha blending for
    // translucent ribbons and trails, and smoothing improves the polyline edges.
    ofSetWindowTitle("Sinuous Waves");
    ofSetFrameRate(60);
    ofSetVerticalSync(true);
    ofEnableAlphaBlending();
    ofEnableSmoothing();
    ofSetCircleResolution(48);

    // Build a tiny radial glow image procedurally.  This avoids shipping an image
    // asset and lets the sketch stamp soft light blooms along each wave.  The
    // texture is white with only alpha falloff; drawWave() tints it per wave.
    glowTexture.allocate(128, 128, OF_IMAGE_COLOR_ALPHA);
    auto& pixels = glowTexture.getPixels();
    const glm::vec2 center(64.0f, 64.0f);
    for (int y = 0; y < glowTexture.getHeight(); ++y) {
        for (int x = 0; x < glowTexture.getWidth(); ++x) {
            // Normalize distance from the center so 0 is the center and 1 is
            // roughly the edge.  Inverting it gives an alpha mask that fades out.
            const float distance = glm::distance(glm::vec2(x, y), center) / 64.0f;
            const float alpha = ofClamp(1.0f - distance, 0.0f, 1.0f);

            // The exponent tightens the bright center and makes the edge fade
            // gently instead of looking like a flat disc.
            const float shaped = std::pow(alpha, 2.8f);
            pixels.setColor(x, y, ofColor(255, 255, 255, static_cast<unsigned char>(160.0f * shaped)));
        }
    }
    glowTexture.update();

    // The trail buffer is the canvas we continuously paint into.  It starts as
    // opaque black, then each update fades it and draws new wave geometry on top.
    trailBuffer.allocate(ofGetWidth(), ofGetHeight(), GL_RGBA);
    clearTrails();

    // Start with the camera centered on the initial window-sized world area.
    // From there, mouse dragging or the arrow/WASD keys can move through the
    // procedural wave field.
    cameraCenter = glm::vec2(ofGetWidth() * 0.5f, ofGetHeight() * 0.5f);

    // Create the initial randomized wave field.
    rebuildWaves();
}

void ofApp::rebuildWaves() {
    // Throw away the old parameter set and reserve enough storage so pushing the
    // new waves does not repeatedly reallocate the vector.
    waves.clear();
    waves.reserve(waveCount);

    for (int i = 0; i < waveCount; ++i) {
        Wave wave;

        // n runs from 0 to 1 across the wave list.  It is used for orderly
        // top-to-bottom placement and hue spacing, while each individual field
        // still gets some randomness.
        const float n = static_cast<float>(i) / static_cast<float>(std::max(1, waveCount - 1));

        // seed and phase make each wave sample different points in the procedural
        // math.  They are the main reason no two ribbons move exactly alike.
        wave.seed = ofRandom(1000.0f);
        wave.phase = ofRandom(TWO_PI);

        // A signed speed gives half the ribbons a reverse drift.  The globalSpeed
        // multiplier in update() scales this later.
        wave.speed = ofRandom(0.24f, 0.92f) * (ofRandomuf() < 0.5f ? -1.0f : 1.0f);

        // These control the visual weight and motion scale of the ribbon.
        wave.amplitude = ofRandom(28.0f, 150.0f);
        wave.thickness = ofRandom(4.0f, 18.0f);
        wave.wavelength = ofRandom(0.0042f, 0.015f);

        // Distribute base positions from near the top to near the bottom, with a
        // little jitter so the rows do not feel mechanically even.
        wave.yBias = ofLerp(0.13f, 0.87f, n) + ofRandom(-0.1f, 0.1f);

        // Pick an evenly distributed base hue, then add a small random offset.
        // Multiplying by 255 maps the 0-1 fractional hue into OF's HSB range.
        wave.hue = wrappedHue(255.0f * std::fmod(n * kGoldenHue + ofRandom(0.2f), 1.0f));
        wave.hueDrift = ofRandom(-22.0f, 22.0f);

        // Lower alpha values make the ribbons layer and blend instead of covering
        // each other completely.
        wave.alpha = ofRandom(88.0f, 180.0f);
        waves.push_back(wave);
    }
}

void ofApp::update() {
    // Pausing freezes the trail buffer as well as the animation time, so the
    // current image remains onscreen unchanged.
    if (paused) {
        return;
    }

    // If the user resizes the window, recreate the offscreen trail buffer at the
    // new pixel dimensions.  Clearing avoids sampling old pixels from the old
    // buffer size.
    if (trailBuffer.getWidth() != ofGetWidth() || trailBuffer.getHeight() != ofGetHeight()) {
        trailBuffer.allocate(ofGetWidth(), ofGetHeight(), GL_RGBA);
        clearTrails();
    }

    // Draw into the FBO instead of directly to the screen.  This lets us preserve
    // and fade previous frames for trails.
    trailBuffer.begin();
    ofEnableAlphaBlending();

    // A mostly transparent black rectangle slowly darkens previous pixels.  With
    // trails disabled, the rectangle is fully opaque and each frame starts clean.
    ofSetColor(0, 0, 0, drawTrails ? 18 : 255);
    ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());

    // All procedural motion is driven by elapsed time.  Scaling it here makes
    // keyboard speed changes affect every sine/noise term consistently.
    const float time = ofGetElapsedTimef() * globalSpeed;
    for (const auto& wave : waves) {
        // The main pass is the solid ribbon.  The second lower-alpha pass is
        // phase-offset, which creates a shimmering companion strand for depth.
        drawWave(wave, time, 0.0f, 1.0f);
        drawWave(wave, time, 1.71f, 0.42f);
    }

    trailBuffer.end();
}

glm::vec2 ofApp::pointOnWave(const Wave& wave, float x, float time, float passOffset) const {
    // Avoid zero dimensions during unusual resize/minimize states.
    const float width = std::max(1, ofGetWidth());
    const float height = std::max(1, ofGetHeight());

    // Normalized horizontal position.  This is useful for noise because it keeps
    // the noise scale independent of the window's pixel width.
    const float u = x / static_cast<float>(width);

    // swim is the moving phase shared by the wave's sine components.  Because
    // speed can be negative, this phase can advance in either direction.
    const float swim = time * wave.speed + wave.phase + passOffset;

    // The centerline is a sum of several wave sizes:
    // - large: the broad body motion
    // - medium: secondary bends moving at a different rate
    // - small: tight ripples riding on top
    // The non-round multipliers avoid obvious repetition between terms.
    const float large = std::sin(x * wave.wavelength + swim);
    const float medium = std::sin(x * wave.wavelength * 2.37f - swim * 1.31f + wave.seed);
    const float small = std::sin(x * wave.wavelength * 5.11f + swim * 0.73f + wave.seed * 0.29f);

    // Smooth signed noise adds an organic current that is not perfectly periodic.
    // It changes slowly over time so the ribbons feel like they swim through a
    // fluid field instead of following a fixed oscillator.
    const float current = ofSignedNoise(u * 2.0f + wave.seed, time * 0.085f + passOffset);

    // The whole ribbon drifts vertically while the layered waves bend around the
    // yBias anchor.  Each term is scaled by either the wave's amplitude or the
    // window height, producing motion that still looks proportional after resize.
    const float drift = std::sin(time * 0.19f + wave.seed) * height * 0.09f;
    const float y = height * wave.yBias + drift
        + large * wave.amplitude
        + medium * wave.amplitude * 0.46f
        + small * wave.amplitude * 0.18f
        + current * height * 0.08f;

    // Return the 2D center point for this x coordinate.  drawWave() turns this
    // centerline into a thick ribbon by computing normals around it.
    return glm::vec2(x, y);
}

glm::vec2 ofApp::worldToScreen(const glm::vec2& world) const {
    // Treat cameraCenter as the world coordinate that should appear in the middle
    // of the window.  Subtracting it recenters world space around the camera,
    // multiplying applies zoom, and adding half the window size moves the origin
    // back to screen coordinates.
    return (world - cameraCenter) * cameraZoom + glm::vec2(ofGetWidth() * 0.5f, ofGetHeight() * 0.5f);
}

glm::vec2 ofApp::screenToWorld(const glm::vec2& screen) const {
    // Inverse of worldToScreen().  This is used for camera panning and for
    // converting each screen-space sample x into the world-space x that should be
    // fed into the procedural wave equation.
    return (screen - glm::vec2(ofGetWidth() * 0.5f, ofGetHeight() * 0.5f)) / cameraZoom + cameraCenter;
}

void ofApp::zoomCamera(float zoomFactor, const glm::vec2& screenAnchor) {
    // Remember which world point is under the cursor before the zoom change.
    // After changing cameraZoom, shift cameraCenter so that same world point is
    // still under the cursor.  This feels much more natural than zooming only
    // around the center of the window.
    const glm::vec2 worldBefore = screenToWorld(screenAnchor);
    cameraZoom = ofClamp(cameraZoom * zoomFactor, 0.22f, 5.0f);
    const glm::vec2 worldAfter = screenToWorld(screenAnchor);
    cameraCenter += worldBefore - worldAfter;
    clearTrails();
}

void ofApp::clearTrails() {
    if (!trailBuffer.isAllocated()) {
        return;
    }

    trailBuffer.begin();
    ofClear(0, 0, 0, 255);
    trailBuffer.end();
}

ofColor ofApp::waveColor(const Wave& wave, float time, float along, float alphaScale) const {
    // Color changes both over time and along the length of a ribbon.  The along
    // offset makes each wave gradient through multiple hues from left to right.
    const float hue = wrappedHue(wave.hue + time * wave.hueDrift + along * 92.0f);

    // Saturation and brightness pulse independently.  These values stay near the
    // bright end of HSB so the image remains vivid even with translucent layers.
    const float saturation = 205.0f + 45.0f * std::sin(time * 0.7f + along * TWO_PI + wave.seed);
    const float brightness = 230.0f + 25.0f * std::sin(time * 1.4f - along * PI);

    // Convert animated HSB components into an ofColor.  All channels are clamped
    // because ofColor stores them as unsigned bytes.
    ofColor color;
    color.setHsb(
        static_cast<unsigned char>(hue),
        static_cast<unsigned char>(ofClamp(saturation, 0.0f, 255.0f)),
        static_cast<unsigned char>(ofClamp(brightness, 0.0f, 255.0f)),
        static_cast<unsigned char>(ofClamp(wave.alpha * alphaScale, 0.0f, 255.0f))
    );
    return color;
}

void ofApp::drawWave(const Wave& wave, float time, float passOffset, float alphaScale) const {
    const int width = std::max(1, ofGetWidth());

    // Horizontal sample spacing in pixels.  Smaller values make smoother geometry
    // but create more vertices.  Ten pixels is smooth enough for these broad,
    // continuously changing ribbons.
    const float step = 10.0f;

    // A triangle strip is an efficient way to draw a continuous ribbon.  Every x
    // sample contributes two vertices, one on each side of the centerline.  The
    // GPU connects those pairs into a connected strip of triangles.
    ofMesh ribbon;
    ribbon.setMode(OF_PRIMITIVE_TRIANGLE_STRIP);

    // Sample slightly beyond the screen edges so thick waves do not visibly pop
    // in right at x=0 or disappear abruptly at the right edge.  xScreen is the
    // pixel column being rendered; worldX is the procedural coordinate seen at
    // that pixel after camera pan and zoom.
    for (float xScreen = -step * 2.0f; xScreen <= width + step * 2.0f; xScreen += step) {
        const float worldX = screenToWorld(glm::vec2(xScreen, 0.0f)).x;
        const float worldStep = step / cameraZoom;

        // Estimate the local direction of the wave by comparing nearby points.
        // This finite difference gives us a tangent without needing analytical
        // derivatives of all the sine/noise terms.
        const glm::vec2 previous = pointOnWave(wave, worldX - worldStep, time, passOffset);
        const glm::vec2 next = pointOnWave(wave, worldX + worldStep, time, passOffset);
        const glm::vec2 point = pointOnWave(wave, worldX, time, passOffset);
        glm::vec2 tangent = next - previous;

        // In the unlikely event the two sampled points are almost identical, use
        // a horizontal tangent to avoid normalizing a near-zero vector.
        if (glm::dot(tangent, tangent) < 0.0001f) {
            tangent = glm::vec2(1.0f, 0.0f);
        }
        tangent = glm::normalize(tangent);

        // Rotate the tangent 90 degrees to get the normal direction.  Offsetting
        // along this normal gives the two edges of the ribbon.
        const glm::vec2 normal(-tangent.y, tangent.x);

        // along is a normalized 0-1 position across the current screen view and
        // drives color gradients and width pulsing.
        const float along = ofMap(xScreen, 0.0f, static_cast<float>(width), 0.0f, 1.0f, true);

        // Pulse the half-width as the wave moves.  This keeps the ribbons from
        // reading as rigid tubes and adds a subtle breathing motion.
        const float pulse = 0.72f + 0.28f * std::sin(time * 2.0f + along * TWO_PI * 4.0f + wave.seed);
        const float halfWidth = wave.thickness * pulse;
        const ofColor color = waveColor(wave, time, along, alphaScale);

        // For a triangle strip, vertex order matters: top edge, bottom edge, then
        // top edge, bottom edge at the next sample, and so on.  Adding the color
        // before each vertex stores a per-vertex gradient in the mesh.
        ribbon.addColor(color);
        ribbon.addVertex(glm::vec3(worldToScreen(point + normal * halfWidth), 0.0f));
        ribbon.addColor(color);
        ribbon.addVertex(glm::vec3(worldToScreen(point - normal * halfWidth), 0.0f));
    }

    ribbon.draw();

    // Draw a faint white center spine over the colored ribbon.  It sharpens the
    // motion path and creates bright highlights where many translucent strands
    // overlap.
    ofSetColor(255, static_cast<int>(95 * alphaScale));
    ofSetLineWidth(1.2f);
    ofPolyline spine;
    for (float xScreen = 0.0f; xScreen <= width; xScreen += step * 1.5f) {
        const float worldX = screenToWorld(glm::vec2(xScreen, 0.0f)).x;
        const glm::vec2 point = worldToScreen(pointOnWave(wave, worldX, time, passOffset));
        // ofPolyline is 3D internally in OF 0.12.1, so pass z=0 explicitly.
        spine.addVertex(point.x, point.y, 0.0f);
    }
    spine.draw();

    // Additive blending makes light accumulate instead of simply replacing the
    // pixels beneath it.  Stamping the radial glow every ~100 pixels creates soft
    // luminous knots moving along each ribbon.
    ofEnableBlendMode(OF_BLENDMODE_ADD);
    for (float xScreen = 0.0f; xScreen <= width; xScreen += 96.0f) {
        const float along = ofMap(xScreen, 0.0f, static_cast<float>(width), 0.0f, 1.0f, true);
        const float worldX = screenToWorld(glm::vec2(xScreen, 0.0f)).x;

        // Offset the glow sample a little in x so the highlights slide along the
        // ribbon rather than sitting exactly on the same mesh vertices.
        const glm::vec2 point = worldToScreen(pointOnWave(wave, worldX + std::sin(time + wave.seed) * 28.0f, time, passOffset));
        const ofColor color = waveColor(wave, time, along, 0.33f * alphaScale);
        ofSetColor(color);

        // Scale the glow by ribbon thickness so heavier ribbons get larger blooms.
        const float size = wave.thickness * 5.5f * cameraZoom;
        glowTexture.draw(point.x - size * 0.5f, point.y - size * 0.5f, size, size);
    }

    // Restore normal alpha blending so later drawing, including the UI overlay,
    // behaves predictably.
    ofEnableBlendMode(OF_BLENDMODE_ALPHA);
}

void ofApp::draw() {
    // The real wave drawing happened in update() into trailBuffer.  draw() is
    // responsible for presenting that buffer and adding final screen-space UI.
    ofBackground(0);
    ofSetColor(255);
    trailBuffer.draw(0, 0);

    // A subtle additive wash keeps the background from becoming pure flat black
    // and helps the bright colors feel suspended in a dark medium.
    ofEnableBlendMode(OF_BLENDMODE_ADD);
    ofSetColor(18, 22, 34, 80);
    ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());
    ofEnableBlendMode(OF_BLENDMODE_ALPHA);

    // Small runtime help overlay.  It is intentionally drawn last so it remains
    // readable over the animated trails, and it can be hidden with h.
    if (showHelp) {
        const std::string info =
            "Sinuous Waves\n"
            "drag/arrows/WASD pan  |  wheel zoom  |  0 reset view\n"
            "space pause  |  r randomize  |  t trails  |  +/- speed  |  h hide";
        ofSetColor(0, 170);
        ofDrawRectangle(18, 18, 590, 78);
        ofSetColor(245);
        ofDrawBitmapString(info, 32, 42);
    }
}

void ofApp::keyPressed(int key) {
    // Keep controls simple and immediate.  These change runtime state only; the
    // generated wave parameters remain in memory until r rebuilds them.
    switch (key) {
        case ' ':
            // Freeze or resume update().  draw() continues to present the current
            // FBO contents, so the paused image remains visible.
            paused = !paused;
            break;
        case 'r':
        case 'R':
            // Generate a completely new composition with different colors,
            // amplitudes, speeds, and vertical placement.
            rebuildWaves();
            break;
        case 't':
        case 'T':
            // Toggle between long fading trails and a clean frame-by-frame render.
            drawTrails = !drawTrails;
            break;
        case 'h':
        case 'H':
            // Hide/show the controls overlay.
            showHelp = !showHelp;
            break;
        case '0':
            // Return to the original window-sized view of the world.
            cameraCenter = glm::vec2(ofGetWidth() * 0.5f, ofGetHeight() * 0.5f);
            cameraZoom = 1.0f;
            clearTrails();
            break;
        case OF_KEY_LEFT:
        case 'a':
        case 'A':
            // Move the camera left in world space.  Dividing by zoom makes one key
            // press feel like a similar screen-distance nudge at any zoom level.
            cameraCenter.x -= 70.0f / cameraZoom;
            clearTrails();
            break;
        case OF_KEY_RIGHT:
        case 'd':
        case 'D':
            // Move the camera right through the generated field.
            cameraCenter.x += 70.0f / cameraZoom;
            clearTrails();
            break;
        case OF_KEY_UP:
        case 'w':
        case 'W':
            // Move upward in world space.
            cameraCenter.y -= 70.0f / cameraZoom;
            clearTrails();
            break;
        case OF_KEY_DOWN:
        case 's':
        case 'S':
            // Move downward in world space.
            cameraCenter.y += 70.0f / cameraZoom;
            clearTrails();
            break;
        case '+':
        case '=':
            // Increase the global time multiplier, clamped so the animation stays
            // controllable and does not jump into extreme speeds.
            globalSpeed = std::min(4.0f, globalSpeed + 0.15f);
            break;
        case '-':
        case '_':
            // Slow down the animation while keeping a small nonzero floor so time
            // still advances unless the user explicitly pauses.
            globalSpeed = std::max(0.05f, globalSpeed - 0.15f);
            break;
        default:
            break;
    }
}

void ofApp::mousePressed(int x, int y, int button) {
    if (button != OF_MOUSE_BUTTON_LEFT) {
        return;
    }

    // Start a drag-pan operation.  Store the previous mouse position so each
    // mouseDragged() event can apply only the incremental movement since the last
    // event.
    draggingCamera = true;
    lastDragScreen = glm::vec2(x, y);
}

void ofApp::mouseDragged(int x, int y, int button) {
    if (!draggingCamera || button != OF_MOUSE_BUTTON_LEFT) {
        return;
    }

    const glm::vec2 currentScreen(x, y);
    const glm::vec2 deltaScreen = currentScreen - lastDragScreen;

    // Dragging the image to the right should reveal world content to the left, so
    // cameraCenter moves opposite the mouse delta.  Divide by zoom because a
    // screen pixel covers fewer world units when zoomed in.
    cameraCenter -= deltaScreen / cameraZoom;
    lastDragScreen = currentScreen;
    clearTrails();
}

void ofApp::mouseReleased(int x, int y, int button) {
    if (button == OF_MOUSE_BUTTON_LEFT) {
        draggingCamera = false;
    }
}

void ofApp::mouseScrolled(int x, int y, float scrollX, float scrollY) {
    if (std::abs(scrollY) < 0.001f) {
        return;
    }

    // Use an exponential-ish multiplier so repeated wheel notches feel smooth and
    // symmetric: scrolling up zooms in, scrolling down zooms out.
    const float zoomFactor = scrollY > 0.0f ? 1.12f : 1.0f / 1.12f;
    zoomCamera(zoomFactor, glm::vec2(x, y));
}

void ofApp::windowResized(int w, int h) {
    // Ignore transient invalid sizes that can occur while a window is minimized or
    // being interactively resized.
    if (w <= 0 || h <= 0) {
        return;
    }

    // Match the FBO to the new window size.  Existing trails are cleared because
    // stretching the old buffer would smear the previous image and make the wave
    // positions inconsistent with the new dimensions.
    trailBuffer.allocate(w, h, GL_RGBA);
    clearTrails();
}
