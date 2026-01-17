#include <ultra64.h>

#include "sm64.h"
#include "game_init.h"
#include "main.h"
#include "audio/external.h"
#include "engine/math_util.h"
#include "area.h"
#include "level_update.h"
#include "save_file.h"
#include "sound_init.h"
#include "level_table.h"
#include "course_table.h"
#include "level_commands.h"
#include "rumble_init.h"
#include "config.h"
#include "emutest.h"
#ifdef SRAM
#include "sram.h"
#endif
#include "puppycam2.h"

#ifdef UNIQUE_SAVE_DATA
u16 MENU_DATA_MAGIC = 0x4849;
u16 SAVE_FILE_MAGIC = 0x4441;
#else
#define MENU_DATA_MAGIC 0x4849
#define SAVE_FILE_MAGIC 0x4441
#endif

//STATIC_ASSERT(sizeof(struct SaveBuffer) == EEPROM_SIZE, "eeprom buffer size must match");

extern struct SaveBuffer gSaveBuffer;

struct WarpCheckpoint gWarpCheckpoint;

s8 gMainMenuDataModified;
s8 gSaveFileModified;

u8 gLastCompletedCourseNum = COURSE_NONE;
u8 gLastCompletedStarNum = 0;
u8 gGotFileCoinHiScore = FALSE;
u8 gCurrCourseStarFlags = 0;

u8 gSpecialTripleJump = FALSE;

u8 backupXorBuffer[sizeof(struct SaveFile)];

#define STUB_LEVEL(_0, _1, courseenum, _3, _4, _5, _6, _7, _8) courseenum,
#define DEFINE_LEVEL(_0, _1, courseenum, _3, _4, _5, _6, _7, _8, _9, _10) courseenum,

s8 gLevelToCourseNumTable[] = {
    #include "levels/level_defines.h"
};
#undef STUB_LEVEL
#undef DEFINE_LEVEL

STATIC_ASSERT(ARRAY_COUNT(gLevelToCourseNumTable) == LEVEL_COUNT - 1,
              "change this array if you are adding levels");
#ifdef EEP
#include "vc_ultra.h"

/**
 * Read from EEPROM to a given address.
 * The EEPROM address is computed using the offset of the destination address from gSaveBuffer.
 * Try at most 4 times, and return 0 on success. On failure, return the status returned from
 * osEepromLongRead. It also returns 0 if EEPROM isn't loaded correctly in the system.
 */
static s32 read_eeprom_data(void *buffer, s32 size) {
    s32 status = 0;

    if (gEepromProbe != 0) {
        s32 triesLeft = 4;
        u32 offset = (u32)((u8 *) buffer - (u8 *) &gSaveBuffer) / 8;

        do {
#if ENABLE_RUMBLE
            block_until_rumble_pak_free();
#endif
            triesLeft--;
            status = (gEmulator & EMU_WIIVC)
                   ? osEepromLongReadVC(&gSIEventMesgQueue, offset, buffer, size)
                   : osEepromLongRead  (&gSIEventMesgQueue, offset, buffer, size);
#if ENABLE_RUMBLE
            release_rumble_pak_control();
#endif
        } while (triesLeft > 0 && status != 0);
    }

    return status;
}

/**
 * Write data to EEPROM.
 * The EEPROM address is computed using the offset of the source address from gSaveBuffer.
 * Try at most 4 times, and return 0 on success. On failure, return the status returned from
 * osEepromLongWrite. Unlike read_eeprom_data, return 1 if EEPROM isn't loaded.
 */
static s32 write_eeprom_data(void *_buffer, s32 size) {
    s32 status = 1;
    u8 *buffer = (u8*)_buffer;

    while (((u32)buffer & 7) != 0) {
        buffer--;
        size++;
    }

    while ((size & 7) != 0) {
        size++;
    }

    if (gEepromProbe != 0) {
        s32 triesLeft = 4;
        u32 offset = (u32)((u8 *) buffer - (u8 *) &gSaveBuffer) >> 3;

        do {
#if ENABLE_RUMBLE
            block_until_rumble_pak_free();
#endif
            triesLeft--;
            status = (gEmulator & EMU_WIIVC)
                   ? osEepromLongWriteVC(&gSIEventMesgQueue, offset, buffer, size)
                   : osEepromLongWrite  (&gSIEventMesgQueue, offset, buffer, size);
#if ENABLE_RUMBLE
            release_rumble_pak_control();
#endif
        } while (triesLeft > 0 && status != 0);
    }

    return status;
}
#endif
#ifdef SRAM
/**
 * Read from SRAM to a given address.
 * The SRAM address is computed using the offset of the destination address from gSaveBuffer.
 * Try at most 4 times, and return 0 on success. On failure, return the status returned from
 * nuPiReadSram. It also returns 0 if SRAM isn't loaded correctly in the system.
 */
static s32 read_eeprom_data(void *buffer, s32 size) {
    s32 status = 0;

    if (gSramProbe != 0) {
        s32 triesLeft = 4;
        u32 offset = (u32)((u8 *) buffer - (u8 *) &gSaveBuffer);

        do {
#if ENABLE_RUMBLE
            block_until_rumble_pak_free();
#endif
            triesLeft--;
            status = nuPiReadSram(offset, buffer, ALIGN4(size));
#if ENABLE_RUMBLE
            release_rumble_pak_control();
#endif
        } while (triesLeft > 0 && status != 0);
    }

    return status;
}

/**
 * Write data to SRAM.
 * The SRAM address is computed using the offset of the source address from gSaveBuffer.
 * Try at most 4 times, and return 0 on success. On failure, return the status returned from
 * nuPiWriteSram. Unlike read_eeprom_data, return 1 if SRAM isn't loaded.
 */
static s32 write_eeprom_data(void *_buffer, s32 size) {
    s32 status = 1;
    u8 *buffer = (u8*)_buffer;

    while (((u32)buffer & 7) != 0) {
        buffer--;
        size++;
    }

    while ((size & 7) != 0) {
        size++;
    }

    if (gSramProbe != 0) {
        s32 triesLeft = 4;
        u32 offset = (u32)((u8 *) buffer - (u8 *) &gSaveBuffer);

        do {
#if ENABLE_RUMBLE
            block_until_rumble_pak_free();
#endif
            triesLeft--;
            status = nuPiWriteSram(offset, buffer, ALIGN4(size));
#if ENABLE_RUMBLE
            release_rumble_pak_control();
#endif
        } while (triesLeft > 0 && status != 0);
    }

    return status;
}
#endif


/**
 * Sum the bytes in data to data + size - 2. The last two bytes are ignored
 * because that is where the checksum is stored.
 */
static u16 calc_checksum(u8 *data, s32 size) {
    u16 chksum = 0;

    while (size-- > 2) {
        chksum += *data++;
    }
    return chksum;
}

/**
 * Verify the signature at the end of the block to check if the data is valid.
 */
static s32 verify_save_block_signature(void *buffer, s32 size, u16 magic) {
    struct SaveBlockSignature *sig = (struct SaveBlockSignature *) ((size - 4) + (u8 *) buffer);

    if (sig->magic != magic) {
        return FALSE;
    }
    if (sig->chksum != calc_checksum(buffer, size)) {
        return FALSE;
    }
    return TRUE;
}

/**
 * Write a signature at the end of the block to make sure the data is valid
 */
static void add_save_block_signature(void *buffer, s32 size, u16 magic) {
    struct SaveBlockSignature *sig = (struct SaveBlockSignature *) ((size - 4) + (u8 *) buffer);

    sig->magic = magic;
    sig->chksum = calc_checksum(buffer, size);
}

static void save_main_menu_data(void) {
    if (gMainMenuDataModified) {
        // Compute checksum
        add_save_block_signature(&gSaveBuffer.menuData, sizeof(gSaveBuffer.menuData), MENU_DATA_MAGIC);

        // Write to EEPROM
        write_eeprom_data(&gSaveBuffer.menuData, sizeof(gSaveBuffer.menuData));

        gMainMenuDataModified = FALSE;
    }
}

static void wipe_main_menu_data(void) {
    bzero(&gSaveBuffer.menuData, sizeof(gSaveBuffer.menuData));

    gMainMenuDataModified = TRUE;
    save_main_menu_data();
}

static void xor_save_file_backup(s32 fileIndex) {
    u8 *bufferA = (u8*)&gSaveBuffer.files[fileIndex];
    u8 *bufferB = (u8*)&gSaveBuffer.files[fileIndex ^ 1];
    u8 *bufferXor = (u8*)&gSaveBuffer.file_backups[fileIndex >> 1];
    u32 i;

    for (i = 0; i < sizeof(struct SaveFile); i++) {
        bufferXor[i] = bufferA[i] ^ bufferB[i];
    }

    // Write destination data to EEPROM
    write_eeprom_data(bufferXor, sizeof(struct SaveFile));
}

static void load_xored_save_file_backup(s32 fileIndex) {
    u8 *bufferOther = (u8*)&gSaveBuffer.files[fileIndex ^ 1];
    u8 *bufferBackup = (u8*)&gSaveBuffer.file_backups[fileIndex >> 1];
    u32 i;

    for (i = 0; i < sizeof(struct SaveFile); i++) {
        backupXorBuffer[i] = bufferBackup[i] ^ bufferOther[i];
    }
}

/**
 * Copy save file data from one backup slot to the other slot.
 */
static void restore_save_file_data(s32 destFileIndex) {
    void *srcBuffer = &backupXorBuffer;
    void *destBuffer = &gSaveBuffer.files[destFileIndex];

    load_xored_save_file_backup(destFileIndex);

    // Verify the xored data, just in case. If invalid, we can't recover - erase the destination file.
    if (!verify_save_block_signature(&backupXorBuffer, sizeof(backupXorBuffer), SAVE_FILE_MAGIC)) {
        save_file_erase(destFileIndex);
        return;
    }

    // Compute checksum on source data
    add_save_block_signature(srcBuffer, sizeof(struct SaveFile), SAVE_FILE_MAGIC);

    // Copy source data to destination slot
    bcopy(srcBuffer, destBuffer, sizeof(struct SaveFile));

    // Write destination data to EEPROM
    write_eeprom_data(&gSaveBuffer.files[destFileIndex],
                      sizeof(gSaveBuffer.files[destFileIndex]));
}

void save_file_do_save(s32 fileIndex) {
    if (gSaveFileModified) {
        // Compute checksum
        add_save_block_signature(&gSaveBuffer.files[fileIndex],
                                 sizeof(gSaveBuffer.files[fileIndex]), SAVE_FILE_MAGIC);

        // Write to EEPROM
        write_eeprom_data(&gSaveBuffer.files[fileIndex], sizeof(gSaveBuffer.files[fileIndex]));

        // Create the xor backup
        xor_save_file_backup(fileIndex);

        gSaveFileModified = FALSE;
    }

    save_main_menu_data();
}

void save_file_erase(s32 fileIndex) {
    bzero(&gSaveBuffer.files[fileIndex], sizeof(gSaveBuffer.files[fileIndex]));

    gSaveFileModified = TRUE;
    save_file_do_save(fileIndex);
}

void save_file_copy(s32 srcFileIndex, s32 destFileIndex) {
    bcopy(&gSaveBuffer.files[srcFileIndex], &gSaveBuffer.files[destFileIndex],
          sizeof(gSaveBuffer.files[destFileIndex]));

    gSaveFileModified = TRUE;
    save_file_do_save(destFileIndex);
}

#ifdef UNIQUE_SAVE_DATA
// This should only be called once on boot and never again.
static void calculate_unique_save_magic(void) {
    u16 checksum = 0;

    for (s32 i = 0; i < 20; i++) {
        checksum += (u16) INTERNAL_ROM_NAME[i] << (i & 0x07);
    }

    MENU_DATA_MAGIC += checksum;
    SAVE_FILE_MAGIC += checksum;
}
#endif

void save_file_load_all(void) {
    s32 file;
    s32 validSlots;

#ifdef UNIQUE_SAVE_DATA
    calculate_unique_save_magic(); // This should only be called once on boot and never again.
#endif

    gMainMenuDataModified = FALSE;
    gSaveFileModified = FALSE;

    bzero(&gSaveBuffer, sizeof(gSaveBuffer));
    read_eeprom_data(&gSaveBuffer, sizeof(gSaveBuffer));

    // Verify the main menu data and wipe it if invalid.
    validSlots = verify_save_block_signature(&gSaveBuffer.menuData, sizeof(gSaveBuffer.menuData), MENU_DATA_MAGIC);
    if (!validSlots)
        wipe_main_menu_data();

    for (file = 0; file < NUM_SAVE_FILES; file += 2) {
        // Verify the 2 slots, and if only one of them is broken, restore it using the other and the xor data.
        validSlots = verify_save_block_signature(&gSaveBuffer.files[file], sizeof(gSaveBuffer.files[file]), SAVE_FILE_MAGIC);
        validSlots |= verify_save_block_signature(&gSaveBuffer.files[file + 1], sizeof(gSaveBuffer.files[file + 1]), SAVE_FILE_MAGIC) << 1;

        switch (validSlots) {
            case 0: // Neither copy is correct
                save_file_erase(file);
                save_file_erase(file + 1);
                break;
            case 1: // Slot A is correct and slot B is incorrect
                restore_save_file_data(file + 1);
                break;
            case 2: // Slot B is correct and slot A is incorrect
                restore_save_file_data(file);
                break;
            case 3: // Both copies are correct
                xor_save_file_backup(file);
                break;
        }
    }
}

#ifdef PUPPYCAM
void puppycam_get_save(void) {
    gPuppyCam.options = gSaveBuffer.menuData.saveOptions;

    gSaveBuffer.menuData.firstBoot = gSaveBuffer.menuData.firstBoot;
#ifdef WIDE
    gConfig.widescreen = save_file_get_widescreen_mode();
#endif

    puppycam_check_save();
}

void puppycam_set_save(void) {
    gSaveBuffer.menuData.saveOptions = gPuppyCam.options;

    gSaveBuffer.menuData.firstBoot = 4;

#ifdef WIDE
    save_file_set_widescreen_mode(gConfig.widescreen);
#endif

    gMainMenuDataModified = TRUE;
    save_main_menu_data();
}

void puppycam_check_save(void) {
    if (gSaveBuffer.menuData.firstBoot != 4) {
        wipe_main_menu_data();
        gSaveBuffer.menuData.firstBoot = 4;
        puppycam_default_config();
        puppycam_set_save();
    }
}
#endif

/**
 * Reload the current save file from its backup copy, which is effectively a
 * a cached copy of what has been written to EEPROM.
 * This is used after getting a game over.
 */
void save_file_reload(void) {
    save_file_load_all();

    gMainMenuDataModified = FALSE;
    gSaveFileModified = FALSE;
}

s32 save_file_exists(s32 fileIndex) {
    return (gSaveBuffer.files[fileIndex].flags & SAVE_FLAG_FILE_EXISTS) != 0;
}

#ifdef COMPLETE_SAVE_FILE
s32 save_file_get_course_star_count(UNUSED s32 fileIndex, UNUSED s32 courseIndex) {
    return 8;
}
#else
s32 save_file_get_course_star_count(s32 fileIndex, s32 courseIndex) {
    s32 count = 0;
    u8 starFlags = save_file_get_star_flags(fileIndex, courseIndex);

    while (starFlags) {
        if (starFlags & 1) {
            count++;
        }
        starFlags >>= 1;
    }
    return count;
}

s32 save_file_get_extra_star_count(s32 fileIndex, s32 courseIndex) {
    s32 count = 0;
    u8 starFlags = gSaveBuffer.files[fileIndex].extraStars[courseIndex];
    
    while (starFlags) {
        if (starFlags & 1) {
            count++;
        }
        starFlags >>= 1;
    }
    return count;
}
#endif

s32 save_file_get_total_star_count(s32 fileIndex, s32 minCourse, s32 maxCourse) {
    s32 count = 0;
    s32 i;

    // Get standard course star count.
    for (; minCourse <= maxCourse; minCourse++) {
        count += save_file_get_course_star_count(fileIndex, minCourse);
    }

    for (i = 0; i < EXTRA_STARS_ARRAY; i++) {
        count += save_file_get_extra_star_count(fileIndex, i);
    }

    // Add castle secret star count.
    count += save_file_get_course_star_count(fileIndex, COURSE_NUM_TO_INDEX(COURSE_NONE));

    return count;
}

void save_file_set_flags(u32 flags) {
    gSaveBuffer.files[gCurrSaveFileNum - 1].flags |= (flags | SAVE_FLAG_FILE_EXISTS);
    gSaveFileModified = TRUE;
}

void save_file_clear_flags(u32 flags) {
    u32 capFlags = SAVE_FLAG_CAP_ON_GROUND | SAVE_FLAG_CAP_ON_KLEPTO | SAVE_FLAG_CAP_ON_UKIKI | SAVE_FLAG_CAP_ON_MR_BLIZZARD;

    if (flags & capFlags)
        flags |= capFlags;
    
    gSaveBuffer.files[gCurrSaveFileNum - 1].flags &= ~flags;
    gSaveBuffer.files[gCurrSaveFileNum - 1].flags |= SAVE_FLAG_FILE_EXISTS;
    gSaveFileModified = TRUE;
}

u32 save_file_get_flags(void) {
#ifdef COMPLETE_SAVE_FILE
    return (SAVE_FLAG_FILE_EXISTS            |
            SAVE_FLAG_HAVE_WING_CAP          |
            SAVE_FLAG_HAVE_METAL_CAP         |
            SAVE_FLAG_HAVE_VANISH_CAP        |
            SAVE_FLAG_UNLOCKED_BASEMENT_DOOR |
            SAVE_FLAG_UNLOCKED_UPSTAIRS_DOOR |
            SAVE_FLAG_DDD_MOVED_BACK         |
            SAVE_FLAG_MOAT_DRAINED           |
            SAVE_FLAG_UNLOCKED_PSS_DOOR      |
            SAVE_FLAG_UNLOCKED_WF_DOOR       |
            SAVE_FLAG_UNLOCKED_CCM_DOOR      |
            SAVE_FLAG_UNLOCKED_JRB_DOOR      |
            SAVE_FLAG_UNLOCKED_BITDW_DOOR    |
            SAVE_FLAG_UNLOCKED_BITFS_DOOR    |
            SAVE_FLAG_UNLOCKED_50_STAR_DOOR  |
            SAVE_FLAG_COLLECTED_TOAD_STAR_1  |
            SAVE_FLAG_COLLECTED_TOAD_STAR_2  |
            SAVE_FLAG_COLLECTED_TOAD_STAR_3  |
            SAVE_FLAG_COLLECTED_MIPS_STAR_1  |
            SAVE_FLAG_COLLECTED_MIPS_STAR_2);
#else
    if (gCurrCreditsEntry != NULL || gCurrDemoInput != NULL) {
        return 0;
    }
    return gSaveBuffer.files[gCurrSaveFileNum - 1].flags;
#endif
}

/**
 * Return the bitset of obtained stars in the specified course.
 * If course is COURSE_NONE, return the bitset of obtained castle secret stars.
 */
#ifdef COMPLETE_SAVE_FILE
u32 save_file_get_star_flags(UNUSED s32 fileIndex, UNUSED s32 courseIndex) {
    return 0xFF;
}
#else
u32 save_file_get_star_flags(s32 fileIndex, s32 courseIndex) {
    if (courseIndex == COURSE_NUM_TO_INDEX(COURSE_NONE)) return SAVE_FLAG_TO_STAR_FLAG(gSaveBuffer.files[fileIndex].flags);
    return gSaveBuffer.files[fileIndex].courseStars[courseIndex];
}
u32 save_file_get_extra_stars(s32 fileIndex, s32 courseIndex) {
    return gSaveBuffer.files[fileIndex].extraStars[courseIndex];
}
#endif

/**
 * Add to the bitset of obtained stars in the specified course.
 * If course is COURSE_NONE, add to the bitset of obtained castle secret stars.
 */
void save_file_set_star_flags(s32 fileIndex, s32 courseIndex, u32 bhvParam) {
    u8 starID = 1 << bhvParam;
    if (bhvParam >= 10) {
        starID = 1 << ((bhvParam - 10) & 7);
        gSaveBuffer.files[fileIndex]
            .extraStars[(bhvParam - 10) << 3] |= starID;
    } else if (courseIndex == COURSE_NUM_TO_INDEX(COURSE_NONE)) {
        gSaveBuffer.files[fileIndex].flags |= STAR_FLAG_TO_SAVE_FLAG(starID);
    } else {
        gSaveBuffer.files[fileIndex]
            .courseStars[courseIndex] |= starID;
    }

    gSaveBuffer.files[fileIndex].flags |= SAVE_FLAG_FILE_EXISTS;
    gSaveFileModified = TRUE;
}

void save_file_set_cap_pos(void) {
    struct SaveFile *saveFile = &gSaveBuffer.files[gCurrSaveFileNum - 1];

    saveFile->capLevel = gCurrLevelNum;
    saveFile->capArea = gCurrAreaIndex;
    save_file_set_flags(SAVE_FLAG_CAP_ON_GROUND);
}

s32 save_file_get_cap_pos(void) {
    struct SaveFile *saveFile = &gSaveBuffer.files[gCurrSaveFileNum - 1];
    s32 flags = save_file_get_flags();

    if (saveFile->capLevel == gCurrLevelNum && saveFile->capArea == gCurrAreaIndex
        && (flags & SAVE_FLAG_CAP_ON_GROUND)) {
        return TRUE;
    }
    return FALSE;
}

void save_file_set_sound_mode(u16 mode) {
    set_sound_mode(mode);
    gSaveBuffer.menuData.soundMode = mode;

    gMainMenuDataModified = TRUE;
    save_main_menu_data();
}

#ifdef WIDE
u32 save_file_get_widescreen_mode(void) {
    return gSaveBuffer.menuData.wideMode;
}

void save_file_set_widescreen_mode(u8 mode) {
    gSaveBuffer.menuData.wideMode = mode;

    gMainMenuDataModified = TRUE;
    save_main_menu_data();
}
#endif

u32 save_file_get_sound_mode(void) {
    if (gSaveBuffer.menuData.soundMode >= SOUND_MODE_COUNT) {
        return 0;
    }

    return gSaveBuffer.menuData.soundMode;
}

void save_file_move_cap_to_default_location(void) {
    if (save_file_get_flags() & SAVE_FLAG_CAP_ON_GROUND) {
        switch (gSaveBuffer.files[gCurrSaveFileNum - 1].capLevel) {
            case LEVEL_SSL:
                save_file_set_flags(SAVE_FLAG_CAP_ON_KLEPTO);
                break;
            case LEVEL_SL:
                save_file_set_flags(SAVE_FLAG_CAP_ON_MR_BLIZZARD);
                break;
            case LEVEL_TTM:
                save_file_set_flags(SAVE_FLAG_CAP_ON_UKIKI);
                break;
        }
        save_file_clear_flags(SAVE_FLAG_CAP_ON_GROUND);
    }
}

#if MULTILANG
void eu_set_language(u16 language) {
    gSaveBuffer.menuData.language = language;
    gMainMenuDataModified = TRUE;
    save_main_menu_data();
}

u32 eu_get_language(void) {
    return gSaveBuffer.menuData.language;
}
#endif

void disable_warp_checkpoint(void) {
    // check_warp_checkpoint() checks to see if gWarpCheckpoint.courseNum != COURSE_NONE
    gWarpCheckpoint.courseNum = COURSE_NONE;
}

/**
 * Checks the upper bit of the WarpNode->destLevel byte to see if the
 * game should set a warp checkpoint.
 */
void check_if_should_set_warp_checkpoint(struct WarpNode *warpNode) {
    if (warpNode->destLevel & WARP_CHECKPOINT) {
        // Overwrite the warp checkpoint variables.
        gWarpCheckpoint.actNum = gCurrActNum;
        gWarpCheckpoint.courseNum = gCurrCourseNum;
        gWarpCheckpoint.levelID = warpNode->destLevel & 0x7F;
        gWarpCheckpoint.areaNum = warpNode->destArea;
        gWarpCheckpoint.warpNode = warpNode->destNode;
    }
}

/**
 * Checks to see if a checkpoint is properly active or not. This will
 * also update the level, area, and destination node of the input WarpNode.
 * returns TRUE if input WarpNode was updated, and FALSE if not.
 */
s32 check_warp_checkpoint(struct WarpNode *warpNode) {
    s16 warpCheckpointActive = FALSE;
    s16 currCourseNum = gLevelToCourseNumTable[(warpNode->destLevel & 0x7F) - 1];

    // gSavedCourseNum is only used in this function.
    if (gWarpCheckpoint.courseNum != COURSE_NONE && gSavedCourseNum == currCourseNum
        && gWarpCheckpoint.actNum == gCurrActNum) {
        warpNode->destLevel = gWarpCheckpoint.levelID;
        warpNode->destArea = gWarpCheckpoint.areaNum;
        warpNode->destNode = gWarpCheckpoint.warpNode;
        warpCheckpointActive = TRUE;
    } else {
        // Disable the warp checkpoint just in case the other 2 conditions failed?
        gWarpCheckpoint.courseNum = COURSE_NONE;
    }

    return warpCheckpointActive;
}
