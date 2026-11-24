#include "syn/platform/win32_window.hpp"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>

namespace syn::platform {

struct Window::Impl {
  HWND hwnd = nullptr;
  bool open = false;
};

static std::wstring widen(const char* s) {
  const int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
  std::wstring w(static_cast<std::size_t>(n > 0 ? n : 1), L'\0');
  if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s, -1, w.data(), n);
  return w;
}

static LRESULT CALLBACK wndproc(HWND h, UINT m, WPARAM w, LPARAM l) {
  if (m == WM_NCCREATE) {
    auto* cs = reinterpret_cast<CREATESTRUCTW*>(l);
    SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
  }
  auto* impl = reinterpret_cast<Window::Impl*>(GetWindowLongPtrW(h, GWLP_USERDATA));
  switch (m) {
    case WM_KEYDOWN:
      if (w == VK_ESCAPE) DestroyWindow(h);
      return 0;
    case WM_CLOSE:
      DestroyWindow(h);
      return 0;
    case WM_DESTROY:
      if (impl) impl->open = false;
      return 0;
    case WM_ERASEBKGND:
      return 1;
    case WM_PAINT: {
      PAINTSTRUCT ps;
      BeginPaint(h, &ps);
      EndPaint(h, &ps);
      return 0;
    }
    default:
      break;
  }
  return DefWindowProcW(h, m, w, l);
}

Window::Window() : impl_(std::make_unique<Impl>()) {}
Window::~Window() {
  if (impl_ && impl_->open && impl_->hwnd) DestroyWindow(impl_->hwnd);
}

bool Window::create(const char* title, int cw, int ch) {
  SetProcessDPIAware();  // evita escalonamento borrado do Windows
  HINSTANCE inst = GetModuleHandleW(nullptr);
  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = wndproc;
  wc.hInstance = inst;
  wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
  wc.lpszClassName = L"SynesthesiaWindow";
  RegisterClassExW(&wc);  // falha benigna se ja registrada

  RECT r{0, 0, cw, ch};
  AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
  const std::wstring wt = widen(title);
  impl_->hwnd = CreateWindowExW(0, wc.lpszClassName, wt.c_str(), WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
                                nullptr, nullptr, inst, impl_.get());
  impl_->open = impl_->hwnd != nullptr;
  return impl_->open;
}

bool Window::pump() {
  MSG msg;
  while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  return impl_->open;
}

void Window::present(const std::uint32_t* pixels, int w, int h) {
  if (!impl_->open) return;
  RECT rc;
  GetClientRect(impl_->hwnd, &rc);
  BITMAPINFO bmi{};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = w;
  bmi.bmiHeader.biHeight = -h;  // top-down
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  HDC dc = GetDC(impl_->hwnd);
  StretchDIBits(dc, 0, 0, rc.right - rc.left, rc.bottom - rc.top, 0, 0, w, h, pixels, &bmi,
                DIB_RGB_COLORS, SRCCOPY);
  ReleaseDC(impl_->hwnd, dc);
}

void Window::set_title(const char* title) {
  if (impl_->open) SetWindowTextW(impl_->hwnd, widen(title).c_str());
}

} // namespace syn::platform
