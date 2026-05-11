# Installing Sinuous Waves in a Hugo Site

This sketch can be embedded in Hugo as a static Emscripten/OpenFrameworks bundle. The build creates an `index.html`, JavaScript, WebAssembly, data files, and support assets under `bin/em/shiny/`.

## 1. Build the Web Bundle

From this project:

```sh
source /etc/profile.d/emscripten.sh
make web OF_ROOT=/home/feoh/src/personal/OpenFrameworks/of_v0.12.1_linux64_gcc6_release
```

The output directory is:

```text
bin/em/shiny/
```

## 2. Copy the Bundle into Hugo

In your Hugo site, create a static asset directory for the sketch:

```sh
mkdir -p static/sketches/sinuous-waves
cp -a /home/feoh/src/personal/shiny/bin/em/shiny/. static/sketches/sinuous-waves/
```

After Hugo builds the site, these files will be served at:

```text
/sketches/sinuous-waves/
```

You can link to the standalone page directly:

```markdown
[Open Sinuous Waves](/sketches/sinuous-waves/)
```

## 3. Embed with an Iframe

Create a Hugo shortcode:

```sh
mkdir -p layouts/shortcodes
```

Create `layouts/shortcodes/sinuous-waves.html`:

```html
<iframe
  src="/sketches/sinuous-waves/"
  title="Sinuous Waves"
  loading="lazy"
  allow="fullscreen; cross-origin-isolated"
  style="width: 100%; height: min(80vh, 820px); border: 0; background: #000;"
></iframe>
```

Use it in content:

```go-html-template
{{</* sinuous-waves */>}}
```

## 4. Configure Headers

The openFrameworks Emscripten target currently uses pthreads. Browsers require cross-origin isolation for pthread-enabled WebAssembly. Configure your host to send these headers for the sketch page and its assets:

```text
Cross-Origin-Opener-Policy: same-origin
Cross-Origin-Embedder-Policy: require-corp
```

For Netlify, add this to `static/_headers`:

```text
/sketches/sinuous-waves/*
  Cross-Origin-Opener-Policy: same-origin
  Cross-Origin-Embedder-Policy: require-corp
```

For nginx:

```nginx
location /sketches/sinuous-waves/ {
    add_header Cross-Origin-Opener-Policy same-origin always;
    add_header Cross-Origin-Embedder-Policy require-corp always;
}
```

For Cloudflare Pages, add `_headers` to Hugo's `static/` directory with the same Netlify-style contents.

### Amazon S3 Static Website Hosting

A plain S3 static website endpoint cannot add arbitrary response headers such as `Cross-Origin-Opener-Policy` and `Cross-Origin-Embedder-Policy`. If you deploy this sketch to an S3 web bucket, put CloudFront in front of the bucket and add the headers with a CloudFront Response Headers Policy.

Recommended shape:

```text
Browser -> CloudFront distribution -> S3 bucket / Hugo static site
```

Create a CloudFront Response Headers Policy with these custom headers:

```text
Cross-Origin-Opener-Policy: same-origin
Cross-Origin-Embedder-Policy: require-corp
```

Attach that policy to the cache behavior that serves the sketch path:

```text
/sketches/sinuous-waves/*
```

If your whole Hugo site can safely run cross-origin isolated, you may attach the same policy to the default cache behavior instead. If not, keep it scoped to the sketch path so the rest of the site is not affected.

When uploading the sketch files to S3, make sure the content types are correct:

```sh
aws s3 sync bin/em/shiny/ s3://YOUR_BUCKET/sketches/sinuous-waves/ \
  --delete \
  --content-type text/html \
  --exclude "*" \
  --include "index.html"

aws s3 sync bin/em/shiny/ s3://YOUR_BUCKET/sketches/sinuous-waves/ \
  --delete \
  --exclude "index.html"
```

If your deployment tool does not infer MIME types correctly, explicitly set these:

```text
.html -> text/html
.js   -> text/javascript
.wasm -> application/wasm
.data -> application/octet-stream
.css  -> text/css
```

After syncing, invalidate the CloudFront path:

```sh
aws cloudfront create-invalidation \
  --distribution-id YOUR_DISTRIBUTION_ID \
  --paths "/sketches/sinuous-waves/*"
```

## 5. Local Testing

Hugo's built-in server may not add the cross-origin isolation headers. For quick local testing, use Emscripten's server:

```sh
cd /home/feoh/src/personal/shiny
make web-run OF_ROOT=/home/feoh/src/personal/OpenFrameworks/of_v0.12.1_linux64_gcc6_release
```

To test inside Hugo with headers, use a local reverse proxy or a static server that can add `COOP` and `COEP`.

## Update Workflow

When the sketch changes:

```sh
cd /home/feoh/src/personal/shiny
make web OF_ROOT=/home/feoh/src/personal/OpenFrameworks/of_v0.12.1_linux64_gcc6_release
cp -a bin/em/shiny/. /path/to/hugo/site/static/sketches/sinuous-waves/
```

Then rebuild or redeploy the Hugo site.
