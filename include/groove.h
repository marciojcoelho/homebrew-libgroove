#ifndef __GROOVE_H__
#define __GROOVE_H__

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/************* global *************/
// call once at the beginning of your program
int groove_init();
// enable/disable logging of errors
#define GROOVE_LOG_QUIET    -8
#define GROOVE_LOG_ERROR    16
#define GROOVE_LOG_WARNING  24
#define GROOVE_LOG_INFO     32
void groove_set_logging(int level);


/************* GrooveFile *************/
typedef struct GrooveFile {
    int dirty; // read-only
    char *filename; // read-only

    void *internals; // hands off
} GrooveFile;

// flags to groove_file_metadata_*
#define GROOVE_TAG_MATCH_CASE      1
#define GROOVE_TAG_DONT_OVERWRITE 16

// If the entry already exists, append to it.  Note that no
// delimiter is added, the strings are simply concatenated.
#define GROOVE_TAG_APPEND         32

typedef void GrooveTag;

const char * groove_tag_key(GrooveTag *tag);
const char * groove_tag_value(GrooveTag *tag);

// you are always responsible for calling groove_close on the returned GrooveFile.
GrooveFile * groove_open(char* filename);
void groove_close(GrooveFile * file);

GrooveTag *groove_file_metadata_get(GrooveFile *file, const char *key,
        const GrooveTag *prev, int flags);
// key entry to add to metadata. will be strdup'd
// value entry to add to metadata. will be strdup'd
//    passing NULL causes existing entry to be deleted.
// return >= 0 on success otherwise an error code < 0
// note that this will not save the file; you must call groove_file_save to do that.
int groove_file_metadata_set(GrooveFile *file, const char *key, const char *value, int flags);

// a comma separated list of short names for the format
const char * groove_file_short_names(GrooveFile *file);

// write changes made to metadata to disk.
// return < 0 on error
int groove_file_save(GrooveFile *file);

// song duration in seconds
double groove_file_duration(GrooveFile *file);


/************* GroovePlayer *************/
typedef struct GroovePlaylistItem {
    // all fields are read-only. modify with methods below.
    struct GroovePlaylistItem * prev;
    GrooveFile * file;
    // a volume adjustment in float format to apply to the file when it plays.
    // This is typically used for ReplayGain.
    // To convert from dB to float, use exp(log(10) * 0.05 * dB_value)
    double gain;
    struct GroovePlaylistItem * next;
} GroovePlaylistItem;

typedef struct GroovePlayer {
    // all fields are read-only. modify using methods below.
    // doubly linked list which is the playlist
    GroovePlaylistItem * playlist_head;
    GroovePlaylistItem * playlist_tail;

    // in float format, defaults to 1.0
    double volume;

    void * internals; // don't touch this
} GroovePlayer;

enum GroovePlayerEventType {
    // when the currently playing track changes.
    GROOVE_PLAYER_EVENT_NOWPLAYING,

    // when the audio device tries to read from an empty buffer
    GROOVE_PLAYER_EVENT_BUFFERUNDERRUN,
};

typedef union GroovePlayerEvent {
    enum GroovePlayerEventType type;
} GroovePlayerEvent;

// you may not create two simultaneous players on the same device
GroovePlayer * groove_create_player();
// this will not call groove_close on any files
void groove_destroy_player(GroovePlayer *player);

void groove_player_play(GroovePlayer *player);
void groove_player_pause(GroovePlayer *player);

void groove_player_seek(GroovePlayer *player, GroovePlaylistItem *item, double seconds);

// once you add a file to the playlist, you must not destroy it until you first
// remove it from the playlist.
// next: the item you will insert before. if it is NULL, you will append to the playlist.
// gain: see GroovePlaylistItem structure. use 0 for no adjustment.
// returns the newly created playlist item.
GroovePlaylistItem * groove_player_insert(GroovePlayer *player, GrooveFile *file,
        double gain, GroovePlaylistItem *next);

// this will not call groove_close on item->file !
// item is destroyed and the address it points to is no longer valid
void groove_player_remove(GroovePlayer *player, GroovePlaylistItem *item);

// get the position of the play head
// both the current playlist item and the position in seconds in the playlist
// item are given. item will be set to NULL if the playlist is empty
// you may pass NULL for item or seconds
void groove_player_position(GroovePlayer *player, GroovePlaylistItem **item, double *seconds);

// get the position of the decode head
// this is typically 200ms ahead of the playhead due to buffering
// same API as groove_player_position
void groove_player_decode_position(GroovePlayer *player, GroovePlaylistItem **item,
        double *seconds);

// return 1 if the player is playing; 0 if it is not.
int groove_player_playing(GroovePlayer *player);


// remove all playlist items
void groove_player_clear(GroovePlayer *player);

// return the count of playlist items
int groove_player_count(GroovePlayer *player);

void groove_player_set_gain(GroovePlayer *player, GroovePlaylistItem *item,
        double gain);

// value is in float format. defaults to 1.0
void groove_player_set_volume(GroovePlayer *player, double volume);

// returns < 0 on error, 0 on no event ready, 1 on got event
int groove_player_event_poll(GroovePlayer *player, GroovePlayerEvent *event);
// returns < 0 on error
int groove_player_event_wait(GroovePlayer *player, GroovePlayerEvent *event);
// returns < 0 on error, 0 on no event ready, 1 on event ready
// if block is 1, block until event is ready
int groove_player_event_peek(GroovePlayer *player, int block);


/************* GrooveReplayGainScan *************/
typedef struct GrooveReplayGainScan {
    // userdata: the same value you passed to groove_replaygainscan_add
    // amount: value between 0 and 1 representing progress
    // optional callback
    void (*file_progress)(void *userdata, double amount);
    // number of seconds to decode before progress callback is called
    double progress_interval;
    // userdata: the same value you passed to groove_replaygainscan_add
    // gain: recommended gain adjustment of this file, in float format
    // peak: peak amplitude of this file, in float format
    void (*file_complete)(void *userdata, double gain, double peak);
    // set this to 1 during a callback if you want to abort the scan
    int abort_request;
    // hands off
    void *internals;
} GrooveReplayGainScan;

// after you create a GrooveReplayGainScan you may set the callbacks and call
// groove_replaygainscan_add
GrooveReplayGainScan * groove_create_replaygainscan();

// userdata will be passed back in callbacks
int groove_replaygainscan_add(GrooveReplayGainScan *scan, GrooveFile *file, void *userdata);

// starts replaygain scanning. blocks until scanning is complete.
// gain: recommended gain adjustment of all files in scan, in float format
// peak: peak amplitude of all files in scan, in float format
int groove_replaygainscan_exec(GrooveReplayGainScan *scan, double *gain, double *peak);

// must be called to cleanup. May not be called during a callback.
void groove_replaygainscan_destroy(GrooveReplayGainScan *scan);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GROOVE_H__ */