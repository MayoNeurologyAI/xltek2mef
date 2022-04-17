#include <time.h>

#ifndef OUTPUT_TO_MEF3
#include "size_types.h"  // defined separately for MEF3, will conflict with this
#endif

//#include "mef_header.h"

#ifdef OUTPUT_TO_MEF2
#include "mef.h"
#endif

#ifdef OUTPUT_TO_MEF3
#include "meflib.h"
#include "write_mef_channel.h"
#endif

#ifndef XLTEK2MAYO_IN
#define XLTEK2MAYO_IN


/* Header Offsets */

#define GUID_OFFSET			0
#define GUID_BYTES			16
#define RAW_DATA_GUID			{0x91, 0x07, 0x0B, 0xCA, 0x95, 0xD7, 0xD0, 0x11, 0xAF, 0x32, 0x00, 0xA0, 0x24, 0x5B, 0x54, 0xA5}

#define FILE_SCHEMA_OFFSET		16	/* 2-byte int */
#define NUM_SUPPORTED_SCHEMATA		2
#define SUPPORTED_SCHEMATA		{8, 9}

#define CREATION_TIME_OFFSET		20	/* 4-byte time_t value (UTC format) */

#define SAMPLESTAMP_OFFSET      356
	
#define PT_SURNAME_OFFSET		32
#define PT_SURNAME_BYTES		80

#define PT_FIRSTNAME_OFFSET		112
#define PT_FIRSTNAME_BYTES		80

#define PT_MIDDLENAME_OFFSET		192
#define PT_MIDDLENAME_BYTES		80

#define PT_ID_OFFSET			272
#define PT_ID_BYTES			80

#define GENERIC_HEADER_END_OFFSET	352
#define SAMP_FREQ_OFFSET		352	/* 8-byte float */

#define NUM_CHANS_OFFSET		360	/* 4-byte int */

#define DELTA_BITS_OFFSET		364	/* 4-byte int */
#define SUPPORTED_DELTA_BITS		8

#define PHYS_TO_STOR_CHAN_MAP_OFFSET	368
#define PHYS_TO_STOR_CHAN_MAP_SIZE	1024	/* PHYS_TO_STOR_CHAN_MAP_SIZE 4-byte ints */

#define HEADBOX_TYPE_ARRAY_OFFSET	4464	/* HEADBOX_TYPE_ARRAY_SIZE 4-byte ints */
#define HEADBOX_TYPE_ARRAY_SIZE		4
//#define NUM_SUPPORTED_HEADBOX_TYPES	2
//#define SUPPORTED_HEADBOX_TYPES		{1, 3}	/* 1 ==> EEG32, 2 ==> EEG128 */
#define NUM_SUPPORTED_HEADBOX_TYPES	4
#define SUPPORTED_HEADBOX_TYPES		{1, 3, 9, 17}	/* 1 ==> EEG32, 2 ==> EEG128 */

#define DISCARD_BITS_OFFSET		4556	/* 4-byte int */

#define SHORTED_CHANS_OFFSET		4560	/* SHORTED_CHAN_ARRAY_SIZE 2-byte ints */
#define SHORTED_CHAN_ARRAY_SIZE		1024

#define SKIP_FACT_OFFSET		6608	/* SKIP_FACT_ARRAY_SIZE 2-byte ints */
#define SKIP_FACT_ARRAY_SIZE		1024
#define NO_SKIP				32767	/* short max (32767, (2^15)-1) is flag for full sampling rate  */

#define SAMPLE_PACKET_OFFSET		8656

#define MAX_CHAN_ALLOCATION   2048  // make this really big, even though we won't use all the channels

#define SECS_PER_BLOCK 5

#define MAX_SLEEP_STAGE_EVENTS 10000  // TBD calculate this based on duration of recording and duration of epoch




/* Typedefs and Structs */

typedef struct {
	si4		file_schema;
	time_t		creation_time;				/* UTC format */
	si1		time_string[26];
	si1		pt_surname[PT_SURNAME_BYTES];
	si1		pt_firstname[PT_FIRSTNAME_BYTES];	
	si1		pt_middlename[PT_MIDDLENAME_BYTES];
	si1		pt_id[PT_ID_BYTES];
	sf8		samp_freq;
	si4		num_system_chans;
	si4		num_recorded_chans;
	si4		num_delta_bits;
	si4		num_headboxes;
	si4		headbox_types[HEADBOX_TYPE_ARRAY_SIZE];
	si4		num_discard_bits;
	sf8		microvolts_per_AD_unit;	
	si4		*rec_to_phys_chan_map;
	} HEADER_INFO;

typedef struct {
    si1     *path;
	si1		*name;
	time_t		time;
    si4     num_recorded_chans;
    si4     headbox_type;
    sf8     samp_freq;
    si4     samplestamp;
    si1     birth_time[128];
    si4     erd_file_counter;
    si4     duplicate_flag;
	} FILE_TIME;

typedef struct {
	si4		samp_stamp;
	ui8		samp_time;
	} SYNC_INFO;

typedef struct {
	si4		samp_num;
	ui8		mayo_time;
	} MAYO_SYNC_INFO;

typedef struct {
	si4		samp_stamp;
	si4		samp_num;
	} ETC_INFO;

typedef struct {
	si1		*event;
    si1     *note_text;  // added for MEF2/MEF3 writing
	ui8		mayo_time;
	si4		rec_len;
	si8     duration;
	si4     id_number;
	} EVENT_PTR;

typedef struct {
    ui8	timestamp;
    si4	*packet_start;
    ui8 packet_size;
} PACKET_TIME;

#ifdef OUTPUT_TO_MEF2

typedef struct BLOCK_INDEX_ELEMENT BLOCK_INDEX_ELEMENT;

struct BLOCK_INDEX_ELEMENT {
    ui8     block_hdr_time;
    ui8     outfile_data_offset;
    ui8     num_entries_processed;
    BLOCK_INDEX_ELEMENT *next;
};

typedef struct DISCONTINUITY_INDEX_ELEMENT DISCONTINUITY_INDEX_ELEMENT;

struct DISCONTINUITY_INDEX_ELEMENT {
    ui8     block_index;
    DISCONTINUITY_INDEX_ELEMENT *next;
};

typedef struct {
    si4     chan_num;
    si4     *raw_data_ptr_start;
    si4     *raw_data_ptr_current;
    ui8     block_hdr_time;
    ui8     block_boundary;
    ui8     last_chan_timestamp;
    ui8     first_chan_timestamp;
    ui8     max_block_size;
    ui8     max_block_len;
    si4     max_data_value_file;
    si4     min_data_value_file;
    ui8     outfile_data_offset;
    ui8     number_of_index_entries;
    ui8     number_of_discontinuity_entries;
    ui8     number_of_samples;
    ui8     block_sample_index;
    si4     discontinuity_flag;
    si4     bit_shift_flag;
    FILE    *out_file;
    FILE    *local_out_file;
    FILE    *out_file_mtf;
    FILE    *local_out_file_mtf;
    ui1*    out_data;
    ui1*    temp_mtf_index;
    si1     temp_file_name[1024];
    si1     local_temp_file_name[1024];
    BLOCK_INDEX_ELEMENT *block_index_head;
    BLOCK_INDEX_ELEMENT *block_index_current;
    DISCONTINUITY_INDEX_ELEMENT *discontinuity_index_head;
    DISCONTINUITY_INDEX_ELEMENT *discontinuity_index_current;
    si4 normal_block_size;
} CHANNEL_STATE;

#endif

typedef struct {
	si1		**chan_comments;
	si1		**chan_names;
	si4		**channel_data;
	si4		curr_sync_idx;
	ui8		current_time;
	FILE_TIME	*data_file_names;
	si1		*data;
	ui8		*data_chan_offsets;
	FILE		**dfps;
	FILE_TIME	*dir_list;
	FILE		*efp;
	ETC_INFO	*etc_info;
	ui8		file_num_samps;
	ui8		flen;
	HEADER_INFO	header_info, header_info_new;
	si1		*input_path;
	sf8		key_samp_interval;
	ui8		last_sync_entry;
	MAYO_SYNC_INFO	*mayo_sync_info;
	//si1		mef_header[MEF_HEADER_LENGTH];
#ifdef OUTPUT_TO_MEF2
    MEF_HEADER_INFO out_header_struct;
#endif
    CHANNEL_STATE **channel_state_struct;
	si1		*montage_file;
	si4		n_dirs;
	si4		n_etc_entries;
	si4		n_mayo_sync_entries;
	si4		n_sync_recs;
	si4		num_files;
	si4		VTC_Record_Length;
	sf8		samp_delta;
	sf8		samp_freq;
	SYNC_INFO	*sync_info;
	ui8		tot_num_samps;
	ui8		tot_time_recs;
	ui8		tot_num_events;
	FILE		**tfps;
    char    **montage_array;
    si1     *output_directory;
    si1     anonymize_output;
    si1     skip_C_channels;
    si1   *skipping_names;
	si4     noprompt;
	si4     convert_video;
	si1     conversion_failed_file[MEF_FULL_FILE_NAME_BYTES];
#ifdef OUTPUT_TO_MEF3
    ANNOTATION_STATE annotation_state;
	si4     video_segment_counter;
	FILE_PROCESSING_STRUCT* proto_metadata_fps;
#endif
	} GLOBALS;


#ifdef OUTPUT_TO_MEF2
void pack_mef_header(MEF_HEADER_INFO *out_header_struct, sf8 secs_per_block, si1 *session_password, si1 *subject_password,
                     si1 *uid, si4 anonymize_flag, si4 dst_flag, si4 bit_shift_flag, sf8 sampling_frequency,
                     si1 *default_first_name, si1 *default_third_name, si1 *default_id, sf8 voltage_conversion_factor);
si4 initialize_mef_channel_data ( CHANNEL_STATE *channel_state, MEF_HEADER_INFO *header_ptr, sf8 secs_per_block,
                                 si1 *chan_map_name, si1 *subject_password, si4 bit_shift_flag, si1 *path, si4 chan_num);
si4 write_mef_channel_data(CHANNEL_STATE *channel_state, MEF_HEADER_INFO *header_ptr, PACKET_TIME *packet_times, si4 *samps,
                           ui8 n_packets_to_process, sf8 secs_per_block);
si4 close_mef_channel_file(CHANNEL_STATE *channel_state, MEF_HEADER_INFO *header_ptr, si1* session_password,
                           si1 *subject_password, sf8 secs_per_block);
#endif

void read_XLTek_header(si1 *data, HEADER_INFO *header_info, si1 patient_anon);
si4 read_epo_file(EVENT_PTR* event_ptrs, GLOBALS* globals, si4 event_cnt, si4 dir_idx);

/* Useful program constants */

#define XLTEK_OVERFLOW_FLAG		0xFFFF
#define XLTEK_SEG_SIZE			((1<<20)*100)		/* 100 MB */
#define XLTEK_TIME_CORRECTION_FACTOR	11644473600		/* Seconds to subtract to convert FILETIME to UTC time (~ 369 years) */

//2 different versions of vtc files, depending on whether video is mpg or avi. Record lengths differ between the two.
#define VTC_GUID_AVI	{0xA1, 0xAC, 0xE3, 0x5B, 0x30, 0xA2, 0xC0, 0x4A, 0xAA, 0x94, 0x49, 0x65, 0x0F, 0x2C, 0xF6, 0xA0}
#define VTC_GUID_MPG	{0xF1, 0xC1, 0x01, 0x5E, 0xA9, 0xF9, 0xD2, 0x11, 0xAD, 0xF3, 0x00, 0x10, 0x4B, 0x8C, 0xC6, 0x5E}
#define VTC_HEADER_LENGTH				20
#define XLTEK_VTC_RECORD_LENGTH_MPG		284
#define XLTEK_VTC_RECORD_LENGTH_AVI		293

#define TIME_ALIGNMENT_SECS		5.0

#define DISCONTINUITY_TIME_THRESHOLD 100000

#endif
