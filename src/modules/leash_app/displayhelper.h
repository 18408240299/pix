#pragma once


class DisplayHelper
{
public:
    static void showLogo();
    static void showMain(int mode, const char *presetName,
                         int airdogMode, int followMode, int landMode);
    static void showMenu(int buttons, int type, int value, const char *presetName,
                         const char *customText = nullptr);
    static void showInfo(int info, int error = 0);
    static void showList(const char **lines, int lineCount, int x, int y);
};
