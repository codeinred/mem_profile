#pragma once

/// See: https://stackoverflow.com/a/33206814
///
/// For a full color table

#define MP_COLOR_Re "\033[0m"
#define MP_COLOR_Bl "\033[30m"
#define MP_COLOR_R "\033[31m"
#define MP_COLOR_G "\033[32m"
#define MP_COLOR_Y "\033[33m"
#define MP_COLOR_B "\033[34m"
#define MP_COLOR_M "\033[35m"
#define MP_COLOR_C "\033[36m"
#define MP_COLOR_W "\033[37m"
#define MP_COLOR_BBl "\033[1;90m"
#define MP_COLOR_BR "\033[1;91m"
#define MP_COLOR_BG "\033[1;92m"
#define MP_COLOR_BY "\033[1;93m"
#define MP_COLOR_BB "\033[1;94m"
#define MP_COLOR_BM "\033[1;95m"
#define MP_COLOR_BC "\033[1;96m"
#define MP_COLOR_BW "\033[1;97m"

// specify gray as an RGB color
#define MP_COLOR_GRAY "\033[32;2;131;148;150m"

namespace mp::colors {
// Reset
constexpr char Re[] = "\033[0m";

// Foreground colors
constexpr char Bl[] = "\033[30m"; ///< Fg: Black
constexpr char R[]  = "\033[31m"; ///< Fg: Red
constexpr char G[]  = "\033[32m"; ///< Fg: Green
constexpr char Y[]  = "\033[33m"; ///< Fg: Yellow
constexpr char B[]  = "\033[34m"; ///< Fg: Blue
constexpr char M[]  = "\033[35m"; ///< Fg: Magenta
constexpr char C[]  = "\033[36m"; ///< Fg: Cyan
constexpr char W[]  = "\033[37m"; ///< Fg: White

// Bright foreground colors (prefer these for anything eye-catching)
constexpr char BBl[] = "\033[1;90m"; ///< Fg: Bright Bold Black
constexpr char BR[]  = "\033[1;91m"; ///< Fg: Bright Bold Red
constexpr char BG[]  = "\033[1;92m"; ///< Fg: Bright Bold Green
constexpr char BY[]  = "\033[1;93m"; ///< Fg: Bright Bold Yellow
constexpr char BB[]  = "\033[1;94m"; ///< Fg: Bright Bold Blue
constexpr char BM[]  = "\033[1;95m"; ///< Fg: Bright Bold Magenta
constexpr char BC[]  = "\033[1;96m"; ///< Fg: Bright Bold Cyan
constexpr char BW[]  = "\033[1;97m"; ///< Fg: Bright Bold White
} // namespace mp::colors
