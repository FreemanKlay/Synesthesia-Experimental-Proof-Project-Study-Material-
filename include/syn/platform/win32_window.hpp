#pragma once
// Janela Win32 nativa que exibe um framebuffer de pixels da CPU (32 bpp, 0xAARRGGBB).
#include <cstdint>
#include <memory>

namespace syn::platform {

class Window {
public:
  struct Impl;
  Window();
  ~Window();
  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;

  bool create(const char* title_utf8, int client_w, int client_h);
  bool pump();  // processa mensagens; false quando a janela foi fechada (ESC ou X)
  void present(const std::uint32_t* pixels, int w, int h);  // StretchDIBits para a area cliente
  void set_title(const char* title_utf8);
private:
  std::unique_ptr<Impl> impl_;
};

} // namespace syn::platform
