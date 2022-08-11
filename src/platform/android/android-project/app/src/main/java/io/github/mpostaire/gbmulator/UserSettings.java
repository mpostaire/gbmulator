package io.github.mpostaire.gbmulator;

public class UserSettings {

    public static final String PREFERENCES = "preferences";

    public static final String EMULATION_MODE = "emulation_mode";
    public static final int EMULATION_MODE_DMG = 1;
    public static final int EMULATION_MODE_CGB = 2;
    public static final int EMULATION_MODE_DEFAULT = EMULATION_MODE_CGB;

    public static final String EMULATION_PALETTE = "emulation_palette";
    public static final int EMULATION_PALETTE_GRAY = 0;
    public static final int EMULATION_PALETTE_ORIG = 1;
    public static final int EMULATION_PALETTE_DEFAULT = EMULATION_PALETTE_GRAY;

    public static final String EMULATION_SPEED = "emulation_speed";
    public static final float EMULATION_SPEED_DEFAULT = 1.0f;

    public static final String EMULATION_SOUND = "emulation_sound";
    public static final float EMULATION_SOUND_DEFAULT = 1.0f;

    public static final String FRAME_SKIP = "frame_skip";
    public static final int FRAME_SKIP_DEFAULT = 0;

    public static final String BUTTONS_OPACITY = "buttons_opacity";
    public static final float BUTTONS_OPACITY_DEFAULT = 1.0f;

    // Portrait buttons positions
    public static final String PORTRAIT_DPAD_X = "portrait_dpad_x";
    public static final int PORTRAIT_DPAD_X_DEFAULT = 0;

    public static final String PORTRAIT_DPAD_Y = "portrait_dpad_y";
    public static final int PORTRAIT_DPAD_Y_DEFAULT = 178;

    public static final String PORTRAIT_A_X = "portrait_a_x";
    public static final int PORTRAIT_A_X_DEFAULT = 140;

    public static final String PORTRAIT_A_Y = "portrait_a_y";
    public static final int PORTRAIT_A_Y_DEFAULT = 188;

    public static final String PORTRAIT_B_X = "portrait_b_x";
    public static final int PORTRAIT_B_X_DEFAULT = 120;

    public static final String PORTRAIT_B_Y = "portrait_b_y";
    public static final int PORTRAIT_B_Y_DEFAULT = 208;

    public static final String PORTRAIT_START_X = "portrait_start_x";
    public static final int PORTRAIT_START_X_DEFAULT = 90;

    public static final String PORTRAIT_START_Y = "portrait_start_y";
    public static final int PORTRAIT_START_Y_DEFAULT = 264;

    public static final String PORTRAIT_SELECT_X = "portrait_select_x";
    public static final int PORTRAIT_SELECT_X_DEFAULT = 40;

    public static final String PORTRAIT_SELECT_Y = "portrait_select_y";
    public static final int PORTRAIT_SELECT_Y_DEFAULT = 264;

    // Landscape buttons positions
    public static final String LANDSCAPE_DPAD_X = "landscape_dpad_x";
    public static final int LANDSCAPE_DPAD_X_DEFAULT = 0;

    public static final String LANDSCAPE_DPAD_Y = "landscape_dpad_y";
    public static final int LANDSCAPE_DPAD_Y_DEFAULT = 60;

    public static final String LANDSCAPE_A_X = "landscape_a_x";
    public static final int LANDSCAPE_A_X_DEFAULT = 264;

    public static final String LANDSCAPE_A_Y = "landscape_a_y";
    public static final int LANDSCAPE_A_Y_DEFAULT = 70;

    public static final String LANDSCAPE_B_X = "landscape_b_x";
    public static final int LANDSCAPE_B_X_DEFAULT = 244;

    public static final String LANDSCAPE_B_Y = "landscape_b_y";
    public static final int LANDSCAPE_B_Y_DEFAULT = 90;

    public static final String LANDSCAPE_START_X = "landscape_start_x";
    public static final int LANDSCAPE_START_X_DEFAULT = 224;

    public static final String LANDSCAPE_START_Y = "landscape_start_y";
    public static final int LANDSCAPE_START_Y_DEFAULT = 140;

    public static final String LANDSCAPE_SELECT_X = "landscape_select_x";
    public static final int LANDSCAPE_SELECT_X_DEFAULT = 30;

    public static final String LANDSCAPE_SELECT_Y = "landscape_select_y";
    public static final int LANDSCAPE_SELECT_Y_DEFAULT = 140;
}
