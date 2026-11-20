#pragma once

#include <cstddef>
#include <cstdlib> // std::getenv, std::strtoul

#if defined(__unix__) || defined(__APPLE__)
#include <sys/ioctl.h> // ioctl, TIOCGWINSZ
#include <unistd.h>    // isatty, STDOUT_FILENO
#endif

namespace ui {

// Ширина терминала в символах - чтобы понять, поместится ли дерево "сверху вниз".
// 1) macOS / Linux: спрашиваем у терминала через ioctl (работает, только если вывод
//    идёт в терминал, а не в файл);
// 2) переменная окружения COLUMNS;
// 3) иначе 120 - типичная ширина окна.
inline std::size_t TerminalWidth() {
#if defined(__unix__) || defined(__APPLE__)
    winsize size{};
    if (isatty(STDOUT_FILENO) && ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_col > 0)
        return size.ws_col;
#endif
    if (const char* columns = std::getenv("COLUMNS")) {
        unsigned long value = std::strtoul(columns, nullptr, 10);
        if (value > 0)
            return value;
    }
    return 120;
}

} // namespace ui
