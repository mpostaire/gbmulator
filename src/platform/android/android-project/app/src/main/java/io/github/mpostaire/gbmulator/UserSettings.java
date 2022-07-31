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

}
