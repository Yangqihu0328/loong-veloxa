/*
 * Veloxa — Hello World Example
 *
 * Renders a simple page with colored boxes to a PPM image file.
 * Demonstrates the full C API pipeline: create → load → render → save.
 *
 * Build:  cmake --build build --target hello_veloxa
 * Run:    ./build/examples/hello_veloxa
 * Output: hello_veloxa_output.ppm
 */

#include "veloxa/api/veloxa_api.h"

#include <cstdio>
#include <cstring>

static const char kHTML[] =
    "<div id='page'>"
    "  <div id='header'>Veloxa Engine</div>"
    "  <div id='content'>"
    "    <div id='box-red'></div>"
    "    <div id='box-green'></div>"
    "    <div id='box-blue'></div>"
    "  </div>"
    "  <div id='footer'></div>"
    "</div>";

static const char kCSS[] =
    "#page {"
    "  width: 400px;"
    "  background-color: white;"
    "}"
    "#header {"
    "  height: 40px;"
    "  background-color: #2C3E50;"
    "}"
    "#content {"
    "  display: flex;"
    "  justify-content: center;"
    "  gap: 20px;"
    "  padding: 30px;"
    "  background-color: #ECF0F1;"
    "}"
    "#box-red {"
    "  width: 80px;"
    "  height: 80px;"
    "  background-color: #E74C3C;"
    "}"
    "#box-green {"
    "  width: 80px;"
    "  height: 80px;"
    "  background-color: #2ECC71;"
    "}"
    "#box-blue {"
    "  width: 80px;"
    "  height: 80px;"
    "  background-color: #3498DB;"
    "}"
    "#footer {"
    "  height: 20px;"
    "  background-color: #2C3E50;"
    "}";

int main() {
  printf("Veloxa %s — Hello World Example\n", vx_version());

  VxEventLoop* loop = vx_event_loop_create_headless();
  VxSurface* surface = vx_surface_create_memory(400, 300);

  VxViewConfig config;
  memset(&config, 0, sizeof(config));
  config.event_loop = loop;
  config.surface = surface;
  config.target_fps = 60;
  config.background_color = 0xFFFFFFFF;

  VxView* view = vx_view_create(&config);
  if (!view) {
    fprintf(stderr, "ERROR: failed to create view\n");
    return 1;
  }

  VxResult res;

  res = vx_view_load_html(view, kHTML, (uint32_t)strlen(kHTML));
  if (res != VX_OK) {
    fprintf(stderr, "ERROR: load_html failed (%d)\n", res);
    return 1;
  }

  res = vx_view_load_css(view, kCSS, (uint32_t)strlen(kCSS));
  if (res != VX_OK) {
    fprintf(stderr, "ERROR: load_css failed (%d)\n", res);
    return 1;
  }

  res = vx_view_update(view);
  if (res != VX_OK) {
    fprintf(stderr, "ERROR: update failed (%d)\n", res);
    return 1;
  }

  printf("Render complete. Saving output...\n");

  vx_view_destroy(view);

  const char* output_path = "hello_veloxa_output.ppm";
  res = vx_surface_save_ppm(surface, output_path);
  if (res != VX_OK) {
    fprintf(stderr, "ERROR: save_ppm failed (%d)\n", res);
    return 1;
  }

  printf("Output saved to: %s\n", output_path);

  vx_surface_destroy(surface);
  vx_event_loop_destroy(loop);

  printf("Done.\n");
  return 0;
}
