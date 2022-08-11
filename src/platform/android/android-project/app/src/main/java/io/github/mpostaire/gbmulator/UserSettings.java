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

    // SCREEN position and size

    public static final String PORTRAIT_SCREEN_X = "portrait_screen_x";
    public static final float PORTRAIT_SCREEN_X_DEFAULT = 0.0f;

    public static final String PORTRAIT_SCREEN_Y = "portrait_screen_y";
    public static final float PORTRAIT_SCREEN_Y_DEFAULT = 0.0f;

    public static final String PORTRAIT_SCREEN_SIZE = "portrait_screen_size";
    public static final float PORTRAIT_SCREEN_SIZE_DEFAULT = 1.0f;

    public static final String LANDSCAPE_SCREEN_X = "landscape_screen_x";
    public static final float LANDSCAPE_SCREEN_X_DEFAULT = 0.0f;

    public static final String LANDSCAPE_SCREEN_Y = "landscape_screen_y";
    public static final float LANDSCAPE_SCREEN_Y_DEFAULT = 0.0f;

    public static final String LANDSCAPE_SCREEN_SIZE = "landscape_screen_size";
    public static final float LANDSCAPE_SCREEN_SIZE_DEFAULT = 1.0f;

    // DIRECTION BUTTONS position and size

    public static final String DIRECTION_BUTTONS_X = "direction_buttons_x";
    public static final int DIRECTION_BUTTONS_X_DEFAULT = 0; // TODO

    public static final String DIRECTION_BUTTONS_Y = "direction_buttons_y";
    public static final int DIRECTION_BUTTONS_Y_DEFAULT = 0; // TODO

    public static final String DIRECTION_BUTTONS_WIDTH = "direction_buttons_width";
    public static final int DIRECTION_BUTTONS_WIDTH_DEFAULT = 32; // TODO

    public static final String DIRECTION_BUTTONS_HEIGHT = "direction_buttons_height";
    public static final int DIRECTION_BUTTONS_HEIGHT_DEFAULT = 32; // TODO

    // TODO A,B,START,SELECT
}
