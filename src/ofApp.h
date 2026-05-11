#pragma once

#include "ofMain.h"

class ofApp : public ofBaseApp {
public:
    // openFrameworks calls these methods automatically as part of the app
    // lifecycle.  setup() runs once, update()/draw() run every frame, and the
    // input/window callbacks run when the user interacts with the app.
    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;
    void mousePressed(int x, int y, int button) override;
    void mouseDragged(int x, int y, int button) override;
    void mouseReleased(int x, int y, int button) override;
    void mouseScrolled(int x, int y, float scrollX, float scrollY) override;
    void windowResized(int w, int h) override;

private:
    // Each Wave stores the parameters for one independent ribbon.  The animation
    // is not a physics simulation; instead, every frame samples several sine
    // functions and noise fields using these values.  Keeping the parameters
    // per-wave gives the image its "many colorful streams swimming around each
    // other" look without needing thousands of particles.
    struct Wave {
        // Random offset used to decorrelate noise and sine terms.  Two waves can
        // share similar speed/amplitude values but still move differently because
        // their seeds shift the underlying functions.
        float seed = 0.0f;

        // Starting phase for the primary sine wave.  This prevents all ribbons
        // from lining up at startup.
        float phase = 0.0f;

        // Signed time multiplier.  Positive waves swim one way; negative waves
        // swim the opposite way, creating visual counterflow.
        float speed = 0.0f;

        // Main vertical displacement, measured in pixels.  Larger amplitudes
        // produce bigger bends and crossings.
        float amplitude = 0.0f;

        // Half-width basis for the rendered ribbon.  The final width pulses
        // slightly over time in drawWave().
        float thickness = 0.0f;

        // Frequency of the primary wave as a function of x.  Lower values make
        // long, lazy curves; higher values make tighter ripples.
        float wavelength = 0.0f;

        // Vertical anchor expressed as a fraction of window height.  This spreads
        // the wave set from top to bottom before the animated offsets are added.
        float yBias = 0.0f;

        // Base hue in openFrameworks' 0-255 HSB color space.
        float hue = 0.0f;

        // Hue change per second.  Positive and negative drift keep neighboring
        // ribbons from cycling through colors in lockstep.
        float hueDrift = 0.0f;

        // Base transparency.  Layers are intentionally translucent so crossings
        // blend into brighter colors.
        float alpha = 0.0f;
    };

    // Randomize the complete set of waves.  Called at startup and when the user
    // presses r.
    void rebuildWaves();

    // Draw one ribbon pass for one wave.  passOffset and alphaScale let each wave
    // be drawn twice with a slightly offset ghost pass for depth and shimmer.
    void drawWave(const Wave& wave, float time, float passOffset, float alphaScale) const;

    // Evaluate the centerline of a wave at a given x coordinate and time.
    glm::vec2 pointOnWave(const Wave& wave, float x, float time, float passOffset) const;

    // Compute the animated HSB color for one location along a wave.
    ofColor waveColor(const Wave& wave, float time, float along, float alphaScale) const;

    // Convert between procedural world coordinates and screen pixels.  The waves
    // live in world space; the camera chooses which part of that world appears in
    // the current window.
    glm::vec2 worldToScreen(const glm::vec2& world) const;
    glm::vec2 screenToWorld(const glm::vec2& screen) const;

    // Zoom around a specific screen-space anchor, usually the mouse cursor.  This
    // keeps the point under the cursor stable while the zoom changes.
    void zoomCamera(float zoomFactor, const glm::vec2& screenAnchor);

    // Clear accumulated trails after camera jumps or resizes so old pixels do not
    // smear across the newly selected view.
    void clearTrails();

    // The active set of independently animated ribbons.
    std::vector<Wave> waves;

    // Offscreen buffer that stores the previous frame.  update() fades it with a
    // translucent black rectangle before drawing the new frame, which creates the
    // luminous motion-trail effect.
    ofFbo trailBuffer;

    // Small radial alpha texture generated at startup and stamped along waves in
    // additive blending mode to create soft glow highlights.
    ofImage glowTexture;

    // Number of ribbons in the field.  Raising this increases density and cost.
    int waveCount = 34;

    // Runtime toggles controlled from keyPressed().
    bool paused = false;
    bool showHelp = true;
    bool drawTrails = true;

    // Global time multiplier.  Individual waves still have their own speed, but
    // this value scales the entire sketch faster or slower.
    float globalSpeed = 1.8f;

    // 2D camera center in procedural world coordinates.  Panning changes this
    // value; the wave functions are sampled at different world x/y positions.
    glm::vec2 cameraCenter = glm::vec2(640.0f, 360.0f);

    // Camera zoom where 1.0 means one world unit maps to one screen pixel.  Values
    // above 1 zoom in; values below 1 zoom out.
    float cameraZoom = 1.0f;

    // Mouse-drag state for click-and-drag panning.
    bool draggingCamera = false;
    glm::vec2 lastDragScreen = glm::vec2(0.0f, 0.0f);
};
