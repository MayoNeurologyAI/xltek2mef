#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <sys/stat.h>
#include <ctype.h>
#ifndef OUTPUT_TO_MEF3
#include "size_types.h"  // defined separately for MEF3, will conflict with this
#endif
#include "XLTek2Mayo.h"
//#include "mef_header.h"

#ifndef _WIN32
#include "dirent.h"
#endif

#define OUTPUT_EVENTS

#ifdef OUTPUT_TO_MEF2
#include "mef.h"
#endif

#ifdef OUTPUT_TO_MEF3
#include "meflib.h"
#endif

si1 override_flag=0;

#define MONTAGE_MAX_CHANS 2048

int	main(int argc, char *argv[])
{
	GLOBALS		globals;
	HEADER_INFO	*hi, *hin, *ht;
	si4		i, j, k, err;
	time_t		temp_time;
	si1		*time_str;
	si8		long_file_time, last_file_time;
	FILE		*tfp;
#ifndef _WIN32
    struct rlimit	lim;
#endif
    ui8 nr;
    si4 slash_location;
    si1    str_temp[1024];

	si4		make_mayo_sync_info();
	void		get_directory_list(), get_initial_header_info(), read_montage_labels(), open_output_files(), fill_basic_header_info();
	void		get_data_file_list(), read_sync_entries(), read_etc_file(), read_data_file(), compare_headers();
	void		read_data(), write_out(), close_files(), write_event_data();
    void   order_directory_list();
    void	print_directory_list(GLOBALS *globals);
    void    fill_header_for_directories(GLOBALS *globals);
    void print_usage_message(char *arg);


#ifndef _WIN32
    getrlimit(RLIMIT_NOFILE, &lim);
    //if (DBUG)printf("cur %d max %d\n", (si4) lim.rlim_cur, (si4)lim.rlim_max );
    
    if (lim.rlim_cur < (2 * MAX_CHAN_ALLOCATION)) {      // enlarge single application open file limit, if needed (system needs about 6 files to itself)
        //printf("Temporarily increased open file limit from %d to ", (si4) lim.rlim_cur - 6);
        lim.rlim_cur = (rlim_t) (2 * MAX_CHAN_ALLOCATION);
        //printf("%d\n", (si4) lim.rlim_cur );
        //sprintf(temp_str, "ulimit -n %d", 2*(si4)lim.rlim_cur); //this didn't work from within the program
        //system(temp_str);
        nr = setrlimit(RLIMIT_NOFILE, &lim);
        if (nr==-1) fprintf(stderr, "Increase file limit failed\n");
        /*if (DBUG) {
            getrlimit(RLIMIT_NOFILE, &lim);//see if it really worked
            //printf("cur %d max %d\n", (si4) lim.rlim_cur, (si4)lim.rlim_max );
        }*/
    }
#endif
    
	/********************************************************************* Usage **************************************************************************/
	if (argc < 2)
    {
        print_usage_message(argv[0]);
        exit(1);
    }
    
    globals.skip_C_channels = 1;
    
    // 1000: pick a really big number, because we don't know yet how many directories there are.  1000 should be sufficient.
    globals.dir_list = (FILE_TIME *) calloc((size_t) 1000, sizeof(FILE_TIME));
    globals.montage_file = NULL;
    globals.proto_metadata_fps = NULL;
    
    i = 1;
    globals.n_dirs = 0;
    globals.output_directory = NULL;
    globals.anonymize_output = 0;
    globals.video_segment_counter = 0;
    globals.noprompt = 0;
    globals.convert_video = 0;
    while (i < argc)
    {
        if (*argv[i] == '-') { //parse options
            switch (argv[i][1]) {
                case 'i': override_flag=1;
                    break;
                case 'o':
                    if (i >= argc)
                    {
                        print_usage_message(argv[0]);
                        exit(1);
                    }
                    globals.output_directory = argv[i+1];
                    i++;
                    break;
                case 'm':
                    if (i >= argc)
                    {
                        print_usage_message(argv[0]);
                        exit(1);
                    }
                    globals.montage_file = argv[i+1];
                    i++;
                    break;
                case 'a':
                    globals.anonymize_output = 1;
                    break;
                case 'f':
                    globals.skip_C_channels = 0;
                    break;
                case 'n':
                    globals.noprompt = 1;
                    break;
                case 'v':
                    globals.convert_video = 1;
                    break;
                    
            }
        }
        else  // process xltek input directory
        {
            
            // check for case where an argument is broken up by a "~ " combination.
            if ((strlen(argv[i]) > 0) && ((i+1) < argc))
            {
                if (argv[i][strlen(argv[i]) - 1] == '~')
                {
                    
                    globals.dir_list[globals.n_dirs].path = (si1 *) calloc((size_t) 1024, sizeof(si1));
                    globals.dir_list[globals.n_dirs].name = (si1 *) calloc((size_t) 1024, sizeof(si1));
                    globals.dir_list[globals.n_dirs].duplicate_flag = 0;
                    sprintf(globals.dir_list[globals.n_dirs].path, ".");
                    sprintf(globals.dir_list[globals.n_dirs].name, "%s %s", argv[i], argv[i+1]);
                    
                    // remove final slash, if it exists
                    if (globals.dir_list[globals.n_dirs].name[strlen(globals.dir_list[globals.n_dirs].name) - 1] == '/')
                        globals.dir_list[globals.n_dirs].name[strlen(globals.dir_list[globals.n_dirs].name) - 1] = 0;
                    
                    //fprintf(stderr, "Looking for slash!\n");
#ifndef _WIN32
                    if (strrchr(globals.dir_list[globals.n_dirs].name, '/') != NULL)
#else
                    if (strrchr(globals.dir_list[globals.n_dirs].name, '\\') != NULL)
#endif
                    {
                        //fprintf(stderr, "Found slash!\n");
                        
                        // break into path and filename
#ifndef _WIN32
                        slash_location = strrchr(globals.dir_list[globals.n_dirs].name, '/') - globals.dir_list[globals.n_dirs].name;
#else
                        slash_location = strrchr(globals.dir_list[globals.n_dirs].name, '\\') - globals.dir_list[globals.n_dirs].name;
#endif
                        strcpy(globals.dir_list[globals.n_dirs].path, globals.dir_list[globals.n_dirs].name);
                        globals.dir_list[globals.n_dirs].path[slash_location] = 0;
                        strcpy(str_temp, globals.dir_list[globals.n_dirs].name);
                        sprintf(globals.dir_list[globals.n_dirs].name, "%s", str_temp+slash_location+1);
                        
                        //fprintf(stderr, "path: %s name: %s\n", globals.dir_list[globals.n_dirs].path, globals.dir_list[globals.n_dirs].name);
                        
                    }
                    
                    globals.n_dirs++;
                    
                    if (globals.n_dirs >= 1000)
                    {
                        fprintf(stderr, "Too many directores (1000+)... update the code to handle more!\n\n");
                        exit(1);
                    }
                    
                    i++;
                    i++;
                    
                    continue;
                }
            }
            
            
            globals.dir_list[globals.n_dirs].path = (si1 *) calloc((size_t) 1024, sizeof(si1));
            globals.dir_list[globals.n_dirs].name = (si1 *) calloc((size_t) 1024, sizeof(si1));
            sprintf(globals.dir_list[globals.n_dirs].path, ".");
            sprintf(globals.dir_list[globals.n_dirs].name, "%s", argv[i]);
            
            // remove final slash, if it exists
            if (globals.dir_list[globals.n_dirs].name[strlen(globals.dir_list[globals.n_dirs].name) - 1] == '/')
                globals.dir_list[globals.n_dirs].name[strlen(globals.dir_list[globals.n_dirs].name) - 1] = 0;
            
            //fprintf(stderr, "Looking for slash!\n");
#ifndef _WIN32
            if (strrchr(globals.dir_list[globals.n_dirs].name, '/') != NULL)
#else
            if (strrchr(globals.dir_list[globals.n_dirs].name, '\\') != NULL)
#endif
            {
                //fprintf(stderr, "Found slash!\n");
                
                // break into path and filename
#ifndef _WIN32
                slash_location = strrchr(globals.dir_list[globals.n_dirs].name, '/') - globals.dir_list[globals.n_dirs].name;
#else
                slash_location = strrchr(globals.dir_list[globals.n_dirs].name, '\\') - globals.dir_list[globals.n_dirs].name;
#endif
                strcpy(globals.dir_list[globals.n_dirs].path, globals.dir_list[globals.n_dirs].name);
                globals.dir_list[globals.n_dirs].path[slash_location] = 0;
                strcpy(str_temp, globals.dir_list[globals.n_dirs].name);
                sprintf(globals.dir_list[globals.n_dirs].name, "%s", str_temp+slash_location+1);
                
                //fprintf(stderr, "path: %s name: %s\n", globals.dir_list[globals.n_dirs].path, globals.dir_list[globals.n_dirs].name);
                
            }
            
            globals.n_dirs++;
            
            if (globals.n_dirs >= 1000)
            {
                fprintf(stderr, "Too many directores (1000+)... update the code to handle more!\n\n");
                exit(1);
            }
        }
        i++;
    }
    
    
    
    if (globals.n_dirs == 0)
    {
        fprintf(stderr, "No XLTEK data directories specified!  Exiting.\n\n");
        exit(1);
    }
	
	/********************************************************* Sort Directory List by Time *****************************************************************/

	order_directory_list(&globals);
    
    fill_header_for_directories(&globals);
		
    print_directory_list(&globals);
    
	/********************************************************* Read Header of First File *******************************************************************/
	
	get_initial_header_info(&globals);
			 
	/****************************************************** Get Channel Labels from Montage File ***********************************************************/
	
	read_montage_labels(&globals);
		 
	/*************************************************************** Fill in basic header info *************************************************************/
	
	fill_basic_header_info(&globals);
		
	/***************************************************************** Open Output Files *******************************************************************/
  
	open_output_files(&globals);
	//debug - check sync times
	//tfp = NULL;
    //if (globals.output_directory == NULL)
    //    sprintf(str_temp, "%s", "time_sync_log.txt");
    //else
    //    sprintf(str_temp, "%s/time_sync_log.txt", globals.output_directory);
    //sprintf(str_temp, get_random_tmp_filename());
	//tfp = fopen(str_temp, "w");
	/*************************************************************** Start Reading Data ********************************************************************/
	
	//globals.data_chan_offsets = (ui8 *) calloc((size_t) globals.header_info.num_recorded_chans, sizeof(ui8));
	//if (globals.data_chan_offsets == NULL) { (void) fprintf(stderr, "\n\nInsufficient memory while allocating channel offsets [l %d]\n\n", __LINE__); exit(1); }
	//for (i = 0; i < globals.header_info.num_recorded_chans; ++i)
	//	globals.data_chan_offsets[i] = MEF_HEADER_LENGTH;  // TBD: this is MEF version 1 stuff that can be removed...

	hi = &globals.header_info; 
	hin = &globals.header_info_new;
	globals.tot_num_samps = 0;
	globals.tot_time_recs = 0;
	globals.tot_num_events = 0;
	globals.last_sync_entry = 0;
	
	globals.current_time = (ui8) time(NULL) * 1000000;

	fprintf(stderr, "\nBeginning data conversion:\n\n");
    
	for (i = 0; i < globals.n_dirs; ++i) {

		// Get data file list for this directory and sort by time	
		get_data_file_list(&globals, i);
		
		// Read in the sync file entries for this directory
		read_sync_entries(&globals, i);
		
		// Read data files for this directory
		globals.curr_sync_idx = 0;
		last_file_time = 0;
		for (j = 0; j < globals.num_files; ++j) {
			
            if (globals.data_file_names[j].samplestamp == -1)
            {
                fprintf(stderr, "Skipping segment\n\n");  fflush(stderr);
                continue;
            }
            
			// Read ".etc" file for ".erd" file 
			read_etc_file(&globals, j);
			
			// Convert sample stamps to Mayo time using sync info & align with true sample numbers
			err = make_mayo_sync_info(&globals);
			free(globals.etc_info); globals.etc_info=NULL;
			if (err) {
				fprintf(stderr, "Skipping segment\n\n");  fflush(stderr);
				free(globals.mayo_sync_info); globals.mayo_sync_info=NULL;
				continue;
			}
			
			// Provide some progress info
			long_file_time = (globals.mayo_sync_info[0].mayo_time + 500000) / 1000000;
			temp_time = (time_t) long_file_time;
			time_str = ctime(&temp_time);
			if (time_str==NULL) {
				fprintf(stderr, "NULL time string in %s: bad timestamp %lld\n\n", __FUNCTION__, long_file_time);
				exit(1);
			}
			time_str[24] = 0;
			fprintf(stderr, "Start time: %s  ", time_str); fflush(stderr);
			//if (tfp != NULL)
			//	fprintf(tfp, " %s\t%ld\t%ld\t %s\n", time_str, long_file_time, long_file_time-last_file_time, globals.data_file_names[j].name);
			last_file_time = long_file_time;

			// read data (".erd") file into memory
			read_data_file(&globals, j);
			
			// read and compare header info
			compare_headers(&globals, hi, hin);
			ht = hin; hin = hi; hi = ht;
			free(hin->rec_to_phys_chan_map); hin->rec_to_phys_chan_map=NULL;
			
			// extract data samples
			read_data(&globals, hi);
			free(globals.data); globals.data=NULL;
						
			// Write out in Mayo format			
			write_out(&globals, hi);
			
			// Clean up for next file
			for (k = 0; k < hi->num_recorded_chans; ++k) 
				if (hi->rec_to_phys_chan_map[k]) {
					free(globals.channel_data[k]);
					globals.channel_data[k] = NULL;
				}
            
			free(globals.channel_data); globals.channel_data=NULL;
			free(globals.mayo_sync_info); globals.mayo_sync_info=NULL;
		}
		// Write out video sync and annotation events for this directory
#ifdef OUTPUT_EVENTS
        fprintf(stderr, "\nConverting and writing event notes.\n\n");
		write_event_data(&globals, i);
#endif

		// Clean up for next directory
// 		free(globals.data_file_names[0].name); globals.data_file_names[0].name=NULL;
		free(globals.data_file_names); globals.data_file_names=NULL;
		free(globals.sync_info); globals.sync_info=NULL;
		
	}        	
		 
	/******************************************************* Fill in remaining header info and close files ***********************************************************/
	
	close_files(&globals, hi);
	//if (tfp != NULL)
	//	fclose(tfp);
	fprintf(stderr, "\n\n");
    //fprintf(stderr, "Process completed successfully.\n");
    remove(globals.conversion_failed_file);
    printf("Process completed successfully.\n");
	return(0);
}


/******************************************************* Sorting Function ****************************************************/

int	compare_times(FILE_TIME *s1, FILE_TIME *s2)
{
	if (s1->time > s2->time)
		return(1);
	if (s1->time < s2->time)
		return(-1);
    // use erd_file_counter as a tie-breaker, prefer more files to come first in the sort
    if (s1->erd_file_counter > s2->erd_file_counter)
    {
        s2->duplicate_flag = 1;
        return(-1);
    }
    if (s1->erd_file_counter < s2->erd_file_counter)
    {
        s1->duplicate_flag = 1;
        return(1);
    }
	return(0);
}

int	compare_samplestamps(FILE_TIME *s1, FILE_TIME *s2)
{
    if (s1->samplestamp > s2->samplestamp)
        return(1);
    if (s1->samplestamp < s2->samplestamp)
        return(-1);
    return(0);
}

/******************************************************* Get Directory List and Sort by Time ****************************************************/

void	order_directory_list(GLOBALS *globals)
{
	si4		i, j, len, n_dirs, fid;
	si1		*path, *tp, str_temp[1024], command[1024], str_temp2[1024];
	ui1		*hb, *lb;
	ui8		flen, long_file_time_r, nr;
	FILE		*fp;
	FILE_TIME	*dir_list;
	int		compare_times();
	struct stat	sb;
    struct stat st;
    FILE *fp_ls;
	
    extern MEF_GLOBALS* MEF_globals;

    // initialize MEF3 library
    (void)initialize_meflib();

    /*
	path = globals->input_path;
	len = strlen(path) - 1;
	if (path[len] == '/') 
		path[len] = 0;
		
	(void) sprintf(command, "ls -F \"%s\" | grep / > temp_dirs", path);
	printf("%s\n", command );
	system(command);
	fp = fopen("temp_dirs", "r");
	if (fp == NULL) { (void) fprintf(stderr, "\n\nCould not create the file \"./temp_dirs\"\n\n"); exit(1); }
	
	fid = fileno(fp);
	fstat(fid, &sb); 
	flen = sb.st_size;
    //fprintf(stderr, "flen=%d", flen);
	
	tp = (si1 *) malloc(flen);
	if (tp == NULL) { (void) fprintf(stderr, "\n\nInsufficient Memory while allocating tp [%s ln %d]\n\n",__FILE__, __LINE__); exit(1); }
	
	nr = fread(tp, sizeof(si1), (size_t) flen, fp);
	if (nr != flen) { (void) fprintf(stderr, "\n\nError reading the file \"./temp_dirs\"\n\n"); exit(1); }
	fclose(fp);
	
	for (i = n_dirs = 0; i < flen; ++i) 
		if (tp[i] == '/') { 
			++n_dirs; 
			tp[i] = 0; 
			tp[++i] = 0; 
		}
			
	dir_list = globals->dir_list = (FILE_TIME *) calloc((size_t) n_dirs, sizeof(FILE_TIME));
	if (dir_list == NULL) { (void) fprintf(stderr, "\n\nInsufficient Memory  while allocating dir_list [%s ln %d]\n\n",__FILE__, __LINE__); exit(1); }
	dir_list[0].name = tp;
	for (n_dirs = 1, i = 0; i < (flen - 2); ++i) 
		if (tp[i] == 0) { 
			i += 2; 
			dir_list[n_dirs++].name = (tp + i); 
		}
	globals->n_dirs = n_dirs;
	system("rm temp_dirs");
     */
    
    fprintf(stderr, "Doing initial reading of directories, please wait...\n");
    
    n_dirs = globals->n_dirs;
    dir_list = globals->dir_list;
    
	for (i = 0; i < n_dirs; ++i) {
        //fprintf(stderr, "%d: %s\n", i, dir_list[i].name);
		sprintf(str_temp, "%s/%s/%s.snc", dir_list[i].path, dir_list[i].name, dir_list[i].name);
		
		len = strlen(str_temp) - 8;
		if (!strncmp(str_temp + len, ".eeg", 4)) { 
			str_temp[len] = 0; 
			strcat(str_temp, ".snc"); 
		}
		
		fp = fopen(str_temp, "rb");
		if (fp == NULL) { (void) fprintf(stderr, "\n\nCould not open the file %s\nExiting.\n", str_temp); exit(1); }
		
		fseek(fp, GENERIC_HEADER_END_OFFSET + 4, SEEK_SET);  // read first sync entry time
		nr = fread(&long_file_time_r, sizeof(si8), (size_t) 1, fp);
		if (nr == 0) {
            printf("Error reading sync file %s\nExiting.\n", str_temp); 
            exit(1); 
        }
		fclose(fp);
		
		dir_list[i].time = long_file_time_r;
        
        // get date of folder creation
        sprintf(str_temp, "%s/%s", dir_list[i].path, dir_list[i].name);
        stat(str_temp, &st);
        strftime(str_temp2, 36, "%m/%d/%Y", localtime(&(st.st_ctime)));
        sprintf(dir_list[i].birth_time, str_temp2);
        //sprintf(dir_list[i].birth_time, st.st_birthtime);
        
        // get number of .erd files in a directory
        /*
        dir_list[i].erd_file_counter = 0;
        char path[1024];
        char sys_command[1024];
        sprintf(sys_command, "/bin/ls -l \"%s/%s/\"*.erd", dir_list[i].path, dir_list[i].name);
        fp_ls = popen(sys_command, "r");
        while (fgets(path, sizeof(path), fp_ls) != NULL)
            dir_list[i].erd_file_counter++;
        pclose(fp);
        //fprintf(stderr, "%d: erd count\n", dir_list[i].erd_file_counter);
        */

        // Use MEF 3 library to do the above operations:
        sprintf(command, "%s/%s/", globals->dir_list[i].path, globals->dir_list[i].name);
        generate_file_list(NULL, &(dir_list[i].erd_file_counter), command, "erd");

        
        fprintf(stderr, ".");  // output to user so the user knows the program isn't stuck, as this loop takes a while sometimes
        
	}
	
	qsort((void *) dir_list, (size_t) n_dirs, sizeof(FILE_TIME), compare_times);
	
	return;
}

void	print_directory_list(GLOBALS *globals)
{
    int i;
    si4 l;
    si4 n_dirs;
    FILE_TIME	*dir_list;
    si1		*time_str;
    time_t temp_time_t;
    ui8 time_correction_factor;
    si1  keyboard_input[256];
    si4 problem_found;
    si4 temp_counter;
    si4 range_beginning;
    si4 range_ending;
    si4 range_excluding;
    FILE_TIME *temp_dir_list;
    
    
start_over:
    
    n_dirs = globals->n_dirs;
    dir_list = globals->dir_list;

    if (globals->noprompt && (globals->n_dirs > 1))
    {
        fprintf(stderr, "When in no_prompt mode, only one directory can be converted.  \nExiting.\n\n");
        exit(1);
    }
    
    time_correction_factor = XLTEK_TIME_CORRECTION_FACTOR * 1000000;
    problem_found = 0;
    
    fprintf(stderr, "\n-------------------------------------------------------\n");
    
    // print start times of each directory
    for (i=0;i<n_dirs;i++)
    {
        // Xltek uses FILETIME format (Win32 epoch): each unit is 100 nanoseconds (.1 microseconds) and uses January 1, 1601 as a base.
        // so, divide by 10 to convert to microseconds (w/ rounding).
        // then, subract the correct offset (~369 years).
        temp_time_t = (dir_list[i].time + 5) / 10;
        temp_time_t -= time_correction_factor;
        
        // convert from microseconds UTC to seconds UTC
        // then, convert to string
        temp_time_t = temp_time_t / 1000000;
        time_str = ctime(&temp_time_t);
        time_str[24] = 0;
        
        // print string
        fprintf(stderr, "%s%i: %s\n    start_time = %s   num_recorded_chans = %d   headbox_type = %d   sampling_freq = %f   erd_file_count = %d%s\n",
                (dir_list[i].duplicate_flag ? "** " : ""),
                i+1,
                dir_list[i].name,
                time_str,
                dir_list[i].num_recorded_chans,
                dir_list[i].headbox_type,
                dir_list[i].samp_freq,
                dir_list[i].erd_file_counter,
                (dir_list[i].duplicate_flag ? " **" : ""));
        
        if (i >= 1)
        {
            if ((dir_list[i].num_recorded_chans != dir_list[i-1].num_recorded_chans) ||
                (dir_list[i].headbox_type != dir_list[i-1].headbox_type) ||
                (dir_list[i].samp_freq != dir_list[i-1].samp_freq))
            {
                problem_found = 1;
            }
        }
    }
    
    fprintf(stderr, "-------------------------------------------------------\n\n");
    
    fprintf(stderr, "** (indicates a possible edited duplicate of an earlier directory)\n");
    if (problem_found)
    {
#ifdef OUTPUT_TO_MEF2
        fprintf(stderr, "There are mismatches in the parameters of the above directories, so cannot convert to a MEF 2 session.\n");
#endif
#ifdef OUTPUT_TO_MEF3
        fprintf(stderr, "There are mismatches in the parameters of the above directories, so cannot convert to a MEF 3 session.\n");
#endif
        
        fprintf(stderr, "Type 'r' to specify a sub-range of the above directories.\n");
        fprintf(stderr, "Type 'e' to exclude a directory.  ");
        fprintf(stderr, "\n:");
    }
    else
    {
#ifdef OUTPUT_TO_MEF2
        fprintf(stderr, "The above directories will be converted to a MEF 2 session.\n");
#endif
#ifdef OUTPUT_TO_MEF3
        fprintf(stderr, "The above directories will be converted to a MEF 3 session.\n");
#endif
        fprintf(stderr, "Type 'r' to sepcify a sub-range of the above directories, type 'e' to exclude a directory, otherwise continuing with all directories.\nContinue? [y]: ");
    }
    if (globals->noprompt)
    {
        sprintf(keyboard_input, "");
        fprintf(stderr, "\n");
    }
    else
        fgets(keyboard_input, 128, stdin);
    l = strlen(keyboard_input) - 1;
    if ((!strncmp(keyboard_input, "N", 1)) || (!strncmp(keyboard_input, "n", 1)))
    {
        fprintf(stderr, "Exiting.\n\n");
        exit(1);
    }
    if ((!strncmp(keyboard_input, "R", 1)) || (!strncmp(keyboard_input, "r", 1)))
    {
    try_again_directory:
        fprintf(stderr, "\nSpecifying a subrange of the above directories.\nYou will be asked for the beginning and ending directory number (as listed ablove)\nBoth the beginning and ending number will be included in the range.\n");
        fprintf(stderr, "Please enter the beginning directory number: ");
        fgets(keyboard_input, 128, stdin);
        l = strlen(keyboard_input) - 1;
        range_beginning = atoi(keyboard_input);
        if (range_beginning < 1 || range_beginning > globals->n_dirs)
            goto try_again_directory;
        fprintf(stderr, "Please enter the ending directory number: ");
        fgets(keyboard_input, 128, stdin);
        l = strlen(keyboard_input) - 1;
        range_ending = atoi(keyboard_input);
        if (range_ending < 1 || range_ending > globals->n_dirs)
            goto try_again_directory;
        if (range_beginning > range_ending)
            goto try_again_directory;
        
        temp_dir_list = (FILE_TIME *) calloc((size_t) 1000, sizeof(FILE_TIME));
        for (i=0;i<globals->n_dirs;i++)
            temp_dir_list[i] = globals->dir_list[i];
        
        temp_counter = 0;
        for (i=0;i<globals->n_dirs;i++)
            if ((i+1 >= range_beginning) && (i+1 <= range_ending))
            {
                globals->dir_list[temp_counter++] = temp_dir_list[i];
            }
        globals->n_dirs = temp_counter;
        free (temp_dir_list);
        goto start_over;
    }
    
    if ((!strncmp(keyboard_input, "E", 1)) || (!strncmp(keyboard_input, "e", 1)))
    {
      try_again_directory_exclude:
        fprintf(stderr, "\nExcluding a directory.\nYou will be asked for directory number (as listed ablove)\n");
        fprintf(stderr, "Please enter the directory number: ");
        fgets(keyboard_input, 128, stdin);
        l = strlen(keyboard_input) - 1;
        range_excluding = atoi(keyboard_input);
        if (range_excluding < 1 || range_excluding > globals->n_dirs)
            goto try_again_directory_exclude;
        
        temp_dir_list = (FILE_TIME *) calloc((size_t) 1000, sizeof(FILE_TIME));
        for (i=0;i<globals->n_dirs;i++)
            temp_dir_list[i] = globals->dir_list[i];
        
        temp_counter = 0;
        for (i=0;i<globals->n_dirs;i++)
            if ((i+1 != range_excluding))
            {
                globals->dir_list[temp_counter++] = temp_dir_list[i];
            }
        globals->n_dirs = temp_counter;
        free (temp_dir_list);
        goto start_over;
    }
    
    if (problem_found)
    {
#ifdef OUTPUT_TO_MEF2
        fprintf(stderr, "There are mismatches in the parameters of the above directories, so cannot convert to a MEF 2 session.\n");
#endif
#ifdef OUTPUT_TO_MEF3
        fprintf(stderr, "There are mismatches in the parameters of the above directories, so cannot convert to a MEF 3 session.\n");
#endif
        fprintf(stderr, "Exiting.\n\n");
        exit(1);
    }
    fprintf(stderr, "\n");
}

/********************************************************* Read Header of First File *******************************************************************/

void	get_initial_header_info(GLOBALS *globals)
{
	si1	str_temp[1024], *data;
	ui8	nr;
	FILE	*fp;
    si4 good_erd_counter;

    good_erd_counter = 0;
    data = NULL;
 
    while (good_erd_counter != -1)
    {
        if (good_erd_counter == 0)
            sprintf(str_temp, "%s/%s/%s.erd", globals->dir_list[0].path, globals->dir_list[0].name, globals->dir_list[0].name);
        else
            sprintf(str_temp, "%s/%s/%s_%03d.erd", globals->dir_list[0].path, globals->dir_list[0].name, globals->dir_list[0].name, good_erd_counter);
        fp = fopen(str_temp, "rb");
        fprintf(stderr, "reading %s.\n", str_temp);
        if (fp == NULL) { 
            (void)fprintf(stderr, "\n\nCould not open the file %s.\n", str_temp); 
            good_erd_counter = -1;
            continue;
            //exit(1); 
        }

        good_erd_counter++;

        data = (si1*)malloc(SAMPLE_PACKET_OFFSET);
        if (data == NULL) { (void)fprintf(stderr, "\n\nInsufficient memory while allocating space for data packet [%s ln %d]\nExiting.\n", __FILE__, __LINE__); exit(1); }

        nr = fread(data, sizeof(si1), (size_t)SAMPLE_PACKET_OFFSET, fp);
        if (nr != SAMPLE_PACKET_OFFSET)
        {
            (void)fprintf(stderr, "\n\nError reading the file %s\n", str_temp);
            fclose(fp);
            free(data);
            continue;
            //exit(1);
        }
        fclose(fp);
        break;
    }
	
	read_XLTek_header(data, &globals->header_info, globals->anonymize_output);
	free(data); data=NULL;
	
	globals->samp_freq = globals->header_info.samp_freq;
	globals->samp_delta = (sf8) 1000000.0 / globals->samp_freq;    // samp interval in microseconds
	
	return;
}

void    fill_header_for_directories(GLOBALS *globals)
{
    si4 i;
    FILE *fp;
    si1 str_temp[1024];
    si1 *data;
    HEADER_INFO temp_header_info;
    ui8 nr;
    si4 good_erd_counter;

    good_erd_counter = 0;
    data = NULL;
    
    for (i=0;i<globals->n_dirs;i++)
    {
        while (good_erd_counter != -1)
        {
            if (good_erd_counter == 0)
            {
#ifndef _WIN32
                sprintf(str_temp, "%s/%s/%s.erd", globals->dir_list[i].path, globals->dir_list[i].name, globals->dir_list[i].name);
#else
                sprintf(str_temp, "%s\\%s\\%s.erd", globals->dir_list[i].path, globals->dir_list[i].name, globals->dir_list[i].name);
#endif
            }
            else
            {
#ifndef _WIN32
                sprintf(str_temp, "%s/%s/%s_%03d.erd", globals->dir_list[i].path, globals->dir_list[i].name, globals->dir_list[i].name, good_erd_counter);
#else
                sprintf(str_temp, "%s\\%s\\%s_%03d.erd", globals->dir_list[i].path, globals->dir_list[i].name, globals->dir_list[i].name, good_erd_counter);
#endif
            }
            good_erd_counter++;

            fp = fopen(str_temp, "rb");
            fprintf(stderr, "reading %s.\n", str_temp);
            if (fp == NULL) { 
                (void)fprintf(stderr, "\n\nCould not open the file %s\nExiting.\n", str_temp); 
                good_erd_counter = -1;
                continue;
            }

            data = (si1*)malloc(SAMPLE_PACKET_OFFSET);
            if (data == NULL) { (void)fprintf(stderr, "\n\nInsufficient memory while allocating space for data packet [%s ln %d]\nExiting.\n", __FILE__, __LINE__); exit(1); }

            nr = fread(data, sizeof(si1), (size_t)SAMPLE_PACKET_OFFSET, fp);
            if (nr != SAMPLE_PACKET_OFFSET) {
                (void)fprintf(stderr, "\n\nError reading data from file %s\n... skipping this file.\n", str_temp);
                fclose(fp);
                free(data);
                continue;

                //(void) fprintf(stderr, "\n\nError reading the file %s\nExiting.\n", str_temp); 
                //exit(1); 
            }
            fclose(fp);
            break;
        }
        
        read_XLTek_header(data, &temp_header_info, globals->anonymize_output);
        free(data); data=NULL;
        
        globals->dir_list[i].samp_freq = temp_header_info.samp_freq;
        globals->dir_list[i].headbox_type = 0;
        if (temp_header_info.num_headboxes >= 1)
            globals->dir_list[i].headbox_type = temp_header_info.headbox_types[0];
        globals->dir_list[i].num_recorded_chans =  temp_header_info.num_recorded_chans;
    }
    
    return;
}

/********************************************************* Get Channel Labels from Montage File *******************************************************/

void write_all_montages_to_file(GLOBALS* globals)
{
    int num_montages;
    int dir_num_for_montages;
    int i;
    int j;
    si1** montage_array_temp;
    si1** prev_montage;
    si4 skip_remaining_names;
    int montage_counter;
    int channel_counter;
    char** find_montage_in_ent(GLOBALS * globals, si4 dir_idx, int instance);
    int count_montage_in_ent(GLOBALS * globals, si4 dir_idx, si4 output_flag);
    int compare_montages(char** input_a, char** input_b);
    int skip_montage_name(char* name);
    FILE* fp;
    si1 str_temp[1024];
    si1 ma_file_name[1024];


    if (globals->output_directory == NULL)
    {
#ifdef OUTPUT_TO_MEF2
        sprintf(str_temp, "mef21/");
#endif
#ifdef OUTPUT_TO_MEF3
        sprintf(str_temp, "mef3");
        sprintf(ma_file_name, "%s.mefd/montages_available.json", str_temp);
        fp = fopen(ma_file_name, "wb+");
#endif
    }
    else
    {
        sprintf(str_temp, "%s/", globals->output_directory);
        sprintf(ma_file_name, "%s.mefd/montages_available.json", globals->output_directory);
        fp = fopen(ma_file_name, "wb+");
    }


    montage_counter = 0;
    prev_montage = NULL;

    fprintf(stderr, "\nWriting all montages to file...\n");
    fprintf(fp, "{\n    \"Montages\": [");
    // cycle through all folders, and find all unique montages.
    for (dir_num_for_montages = 0; dir_num_for_montages < globals->n_dirs; dir_num_for_montages++)
    {
        num_montages = count_montage_in_ent(globals, dir_num_for_montages, 0 /* don't output*/);

        for (i = 0; i < num_montages; i++)
        {

            montage_array_temp = find_montage_in_ent(globals, dir_num_for_montages, i + 1);

            // dono't print montage if it is identical to the previous one.
            if (prev_montage != NULL)
            {
                if (compare_montages(prev_montage, montage_array_temp))
                    continue;
            }

            channel_counter = 0;

            //fprintf(stderr, "montage %d:\n\n", i);
            if (montage_counter > 0)
                fprintf(fp, ",");
            fprintf(fp, "\n        {\n");
            fprintf(fp, "            \"id\": \"%d\",\n", montage_counter++);
            fprintf(fp, "            \"channel_names\": [\n");

            skip_remaining_names = 0;
            for (j = 0; j < MONTAGE_MAX_CHANS; j++)
            {
                // print space between names
                //if ((j > 0) && (montage_array_temp[j - 1] != NULL))
                //    fprintf(stderr, " ");

                //  fprintf(stderr, "got here 2\n");


                  // print names
                if (montage_array_temp[j] != NULL)
                {
                    if (globals->skip_C_channels == 1 && skip_remaining_names == 0)
                    {
                        if (skip_montage_name(montage_array_temp[j]))
                        {
                            skip_remaining_names = 1;
                            //fprintf(stderr, "\n     skipping: (");
                        }
                    }

                    if (channel_counter != 0)
                        fprintf(fp, ", ");
                    fprintf(fp, "\"%s\"", montage_array_temp[j]);
                    
                    channel_counter++;
                }
                //else
                //    break;
                //fprintf(stderr, "got here 2a\n");
            }
            //if (skip_remaining_names == 1)
            //    fprintf(stderr, ")");

            //fprintf(stderr, "got here 3\n");
           //if (i < (num_montages - 1))
             //   fprintf(stderr, "\n\n");

            prev_montage = montage_array_temp;


            fprintf(fp, "\n            ]\n");
            fprintf(fp, "        }");

        }
    }

    fprintf(fp, "\n    \]\n}\n");

    fclose(fp);
    fprintf(stderr, "Wrote all unique montages to file montages_available.json\n");

}


void	read_montage_labels(GLOBALS *globals)
{
	si4	i, j, num_system_chans, num_recorded_chans, phys_chan, chan_idx;
    si4 l;
	si1	**chan_names, str_temp[1024];
	si8	nr;
	FILE	*fp;
    si4 highest_chan_in_montage;
    int num_montages;
    char** find_montage_in_ent(GLOBALS *globals, si4 dir_idx, int instance);
    int count_montage_in_ent(GLOBALS *globals, si4 dir_idx, si4 output_flag);
    si1 keyboard_input[256];
    si1 keyboard_input2[256];
    si1 **montage_array_temp;
    si1 **prev_montage;
    si4 montage_selected;
    si4 dir_num_for_montages;
    si4 skip_remaining_names;
    int compare_montages(char **input_a, char **input_b);
    int skip_montage_name(char *name);
    void write_all_montages_to_file(GLOBALS * globals);
	
    // default montage_array to NULL.  Will only be set to something else if we are creating a new montage file.
    globals->montage_array = NULL;
    
	//chan_names = globals->chan_names = (si1 **) calloc((size_t) globals->header_info.num_system_chans, sizeof(si1 *));
    chan_names = globals->chan_names = (si1 **) calloc((size_t) MAX_CHAN_ALLOCATION, sizeof(si1 *));
	if (chan_names == NULL) { (void) fprintf(stderr, "\n\nInsufficient memory  while allocating chan_names [%s ln %d]\nExiting.\n",__FILE__, __LINE__); exit(1); }
    
    globals->skipping_names = (si1 *) calloc((size_t) MAX_CHAN_ALLOCATION, sizeof(si1));
	
	num_system_chans = globals->header_info.num_system_chans;
	//for (i = 0; i < num_system_chans; ++i) {
    for (i = 0; i < MAX_CHAN_ALLOCATION; ++i) {
		chan_names[i] = (si1 *) calloc((size_t) 16, sizeof(si1));
		if (chan_names[i] == NULL) { (void) fprintf(stderr, "\n\nInsufficient memory  while allocating tp [%s ln %d]\nExiting.\n",__FILE__, __LINE__); exit(1); }
//		printf("chan_names[%d] %p\n", i, chan_names);
        sprintf(chan_names[i], "channel_%03d", i+1);

	}

    // if a montage file is specified, use those names.  Otherwise stick with defaults which we just set above.
    if (globals->montage_file != NULL)
    {
        fp = fopen(globals->montage_file, "rb");
        if (fp == NULL) { (void) fprintf(stderr, "\n\nCould not open the montage file %s\n\n", globals->montage_file); exit(1); }
        
        highest_chan_in_montage = 0;
        while ((nr = fscanf(fp, "%d%s", &phys_chan, str_temp)) == 2)
        {
            if (phys_chan > highest_chan_in_montage)
                highest_chan_in_montage = phys_chan;
            (void) strcpy(chan_names[--phys_chan], str_temp);
        }
        
        if (highest_chan_in_montage != globals->header_info.num_system_chans)
            fprintf(stderr, "\n** WARNING: recording contains %d channels, and montage file contains %d channels.\n",
                    globals->header_info.num_system_chans,
                    highest_chan_in_montage);
        
        fclose(fp);
    }
    // if no montage file is specified, then figure out what to do...
    else
    {
        num_montages = count_montage_in_ent(globals, 0, 0 /* don't output*/);
        
        // use last montage as default
        globals->montage_array = find_montage_in_ent(globals, 0, num_montages);
        // use first montage as default
        //globals->montage_array = find_montage_in_ent(globals, 0, 1);
        
        // test print montage:
        if (globals->montage_array != NULL)
        {
            skip_remaining_names = 0;
            fprintf(stderr, "\n");
            
            for (i=0;i<MONTAGE_MAX_CHANS;i++)
            {
                // print space between names
                if ((i > 0) && (globals->montage_array[i-1] != NULL))
                    fprintf(stderr, " ");
                
                // print names
                if (globals->montage_array[i] != NULL)
                {
                    if (globals->skip_C_channels == 1 && skip_remaining_names == 0)
                    {
                        if (skip_montage_name(globals->montage_array[i]))
                        {
                            skip_remaining_names = 1;
                            fprintf(stderr, "\n     skipping: (");
                        }
                    }
                    
                    fprintf(stderr, "%s", globals->montage_array[i]);
                }
                //else
                //    break;
            }
            if (skip_remaining_names == 1)
                fprintf(stderr, ")");
            fprintf (stderr, "\n");
            count_montage_in_ent(globals, 0, 1 /* do output */);  // just to print out how many montages there are
            
            fprintf(stderr, "Would you like to use the montage listed above? [y]: ");
          
            if (globals->noprompt)
            {
                sprintf(keyboard_input, "Y");
                fprintf(stderr, "\n");
            }
            else
                fgets(keyboard_input, 128, stdin);
            l = strlen(keyboard_input) - 1;
            if ((!strncmp(keyboard_input, "N", 1)) || (!strncmp(keyboard_input, "n", 1)))
            {
                dir_num_for_montages = 0;
                
                
            next_directory:
                
                prev_montage = NULL;
  
                fprintf(stderr, "\n-------------------------------------------------------\n");
                
                fprintf(stderr, "Searching montages in directory: %s\n\n", globals->dir_list[dir_num_for_montages].name);
                
                num_montages = count_montage_in_ent(globals, dir_num_for_montages, 0 /* don't output*/);
                
                for (i=0;i<num_montages;i++)
                {
                    
                    montage_array_temp = find_montage_in_ent(globals, dir_num_for_montages, i+1);
                    
                    // dono't print montage if it is identical to the previous one.
                    if (prev_montage != NULL)
                    {
                        if (compare_montages(prev_montage, montage_array_temp))
                            continue;
                    }
                    
                    fprintf(stderr, "montage %d:\n\n", i);
                    
                    //fprintf(stderr, "got here 1\n");
                    
                    skip_remaining_names = 0;
                    for (j=0;j<MONTAGE_MAX_CHANS;j++)
                    {
                        // print space between names
                        if ((j > 0) && (montage_array_temp[j-1] != NULL))
                            fprintf(stderr, " ");
                        
                      //  fprintf(stderr, "got here 2\n");
                        
    
                        // print names
                        if (montage_array_temp[j] != NULL)
                        {
                            if (globals->skip_C_channels == 1 && skip_remaining_names == 0)
                            {
                                if (skip_montage_name(montage_array_temp[j]))
                                {
                                    skip_remaining_names = 1;
                                    fprintf(stderr, "\n     skipping: (");
                                }
                            }
                            
                            fprintf(stderr, "%s", montage_array_temp[j]);
                        }
                        //else
                        //    break;
                        //fprintf(stderr, "got here 2a\n");
                    }
                    if (skip_remaining_names == 1)
                        fprintf(stderr, ")");
                    
                    //fprintf(stderr, "got here 3\n");
                    if (i < (num_montages-1))
                        fprintf(stderr, "\n\n");
                    
                    prev_montage = montage_array_temp;
                    
                }
                
                fprintf(stderr, "\n-------------------------------------------------------\n\n");
                
                fprintf(stderr, "Select which montage number from above you would like to use, or type \"more\" for more choices: ");
            try_again:
                fgets(keyboard_input2, 128, stdin);
                l = strlen(keyboard_input2) - 1;
                if ((!strncmp(keyboard_input2, "M", 1)) || (!strncmp(keyboard_input2, "m", 1)))
                {
                    dir_num_for_montages++;
                    if (dir_num_for_montages >= globals->n_dirs)
                    {
                        fprintf(stderr, "\nThere are no more available montages!  Going back to beginning of directory list.\n");
                        dir_num_for_montages = 0;
                    }
                    goto next_directory;
                }
                montage_selected = atoi(keyboard_input2);
                fprintf(stderr,"Montage selected: %d\n", montage_selected);
                if ((montage_selected < 0) || (montage_selected+1 >= num_montages))
                {
                    fprintf(stderr, "Not a valid montage number, try again: ");
                    goto try_again;
                }
                
                globals->montage_array = find_montage_in_ent(globals, dir_num_for_montages, montage_selected+1);
                
                // using selected montage...
                for (j=0;j<MONTAGE_MAX_CHANS;j++)
                {
                    if (globals->montage_array[j] != NULL)
                        strcpy(chan_names[j], globals->montage_array[j]);
                }
               
            }
            else
            {
                // using default montage...
                for (i=0;i<MONTAGE_MAX_CHANS;i++)
                {
                    if (globals->montage_array[i] != NULL)
                        strcpy(chan_names[i], globals->montage_array[i]);
                }
            }
        }
        else
        // no montage was found
        {
            fprintf(stderr, "\n** WARNING: no montages were found in the first input directory.  **\n");
            // do something else?
        }
    }

	/* remove entries from rec_to_phys_chan_map that have no name - indicates recorded but do not contain desired data */
    // also remove entries that we deem to be skipped, eg. "C100" or "C200".
	num_recorded_chans = globals->header_info.num_recorded_chans;

	for (i = 0; i < num_recorded_chans; ++i)
    {
		chan_idx = globals->header_info.rec_to_phys_chan_map[i] - 1;
        
        // check to see if we are done with the remainder of the montage.
        if (globals->skip_C_channels == 1)
        {
            if (skip_montage_name(chan_names[chan_idx]))
                globals->skipping_names[chan_idx] = 1;
        }
        
        // skip it if it doesn't have a name
		if (!chan_names[chan_idx][0])
			globals->header_info.rec_to_phys_chan_map[i] = 0;
	}
    
    skip_remaining_names = 0;
    for (i=0;i<MAX_CHAN_ALLOCATION;i++)
    {
        if (globals->skipping_names[i] == 1)
            skip_remaining_names = 1;
        
        if (skip_remaining_names == 1)
            globals->skipping_names[i] = 1;
    }
		
	return;
}

/***************************************************************** Open Output Files *******************************************************************/

void	open_output_files(GLOBALS *globals)
{
	si4	i, num_recorded_chans, *rec_to_phys_chan_map, chan_idx;
	si1	str_temp[1024], **chan_names, *mef_header;
	FILE	**dfps, **tfps;
    FILE    *montage_file;
    int lpf_value;
    FILE* fp;
    
#ifdef OUTPUT_TO_MEF3
    extern MEF_GLOBALS	*MEF_globals;

    // initialize MEF3 library
    (void) initialize_meflib();

    // turn off timestamp offsetting if data is not encrypted
    if (globals->anonymize_output == 1)
    {
        MEF_globals->recording_time_offset_mode = RTO_IGNORE;
    }
    MEF_globals->recording_time_offset_mode = RTO_IGNORE;  // always turn off offsetting timestamps, for now, re-evaluate later
#endif

    if (globals->output_directory == NULL)
    {
#ifdef OUTPUT_TO_MEF2
        sprintf(str_temp, "mkdir mef21 2> /dev/null");
#endif
#ifdef OUTPUT_TO_MEF3
#ifndef _WIN32
        sprintf(str_temp, "mkdir mef3.mefd 2> /dev/null");
#else
        sprintf(str_temp, "mkdir mef3.mefd  > nul 2> nul");
#endif
#endif
    }
    else
    {
#ifdef OUTPUT_TO_MEF2
        sprintf(str_temp, "mkdir -p %s 2> /dev/null", globals->output_directory);
#endif
#ifdef OUTPUT_TO_MEF3
#ifndef _WIN32
        sprintf(str_temp, "mkdir -p %s.mefd 2> /dev/null", globals->output_directory);
#else
        sprintf(str_temp, "mkdir \"%s.mefd\"  > nul 2> nul", globals->output_directory);
#endif
#endif
    }
    system(str_temp);  // subdirectroy already exists

	
	num_recorded_chans = globals->header_info.num_recorded_chans;
    
    globals->channel_state_struct = (CHANNEL_STATE**) calloc((size_t) num_recorded_chans, sizeof(CHANNEL_STATE*));
    
    rec_to_phys_chan_map = globals->header_info.rec_to_phys_chan_map;
    chan_names = globals->chan_names;
    
    if (globals->output_directory == NULL)
    {
#ifdef OUTPUT_TO_MEF2
        sprintf(str_temp, "mef21/");
#endif
#ifdef OUTPUT_TO_MEF3
        sprintf(str_temp, "mef3");
        sprintf(globals->conversion_failed_file, "%s.mefd/conversion_failed.txt", str_temp);
        fp = fopen(globals->conversion_failed_file, "wb+");
        fclose(fp);
#endif
    }
    else
{
        sprintf(str_temp, "%s/", globals->output_directory);
        sprintf(globals->conversion_failed_file, "%s.mefd/conversion_failed.txt", globals->output_directory);
        fp = fopen(globals->conversion_failed_file, "wb+");
        fclose(fp);
}

    write_all_montages_to_file(globals);

    fprintf(stderr, "\nCreating EEG output channel directories...\n");

    
    for (i=0;i<num_recorded_chans; i++)
    {
        globals->channel_state_struct[i] = (CHANNEL_STATE*) calloc((size_t) 1, sizeof(CHANNEL_STATE));
        
        if (rec_to_phys_chan_map[i]) {
            chan_idx = rec_to_phys_chan_map[i] - 1;

            if (globals->skipping_names[chan_idx] == 1)
            {
                globals->channel_state_struct[i] = NULL;
                continue;
            }

            lpf_value = -1.0;
            // The following comes from the table on page 25 of the document:
            // "Natus_014556 REV 13 - Quantum User Service Manual.pdf", copyright 2016
            if (globals->header_info.num_headboxes >= 1)
            {
                if (globals->header_info.headbox_types[0] == 20)  // Quantum headbox
                {
                    if (globals->samp_freq < 500)
                        lpf_value = 109.0;  // when samp_freq == 256
                    else if (globals->samp_freq < 1000)
                        lpf_value = 219.0;  // when samp_freq == 512
                    else if (globals->samp_freq < 2000)
                        lpf_value = 439.0;  // when samp_freq == 1024
                    else if (globals->samp_freq < 4000)
                        lpf_value = 878.0;  // when samp_freq == 2048
                    else if (globals->samp_freq < 8000)
                        lpf_value = 1757.0;  // when samp_freq == 4096
                    else if (globals->samp_freq < 16000)
                        lpf_value = 3514.0;   // when samp_freq == 8192
                    else if (globals->samp_freq < 20000)
                        lpf_value = 4288.0;   // when samp_freq == 16384

                }
            }
            
#ifdef OUTPUT_TO_MEF2
            initialize_mef_channel_data( globals->channel_state_struct[i], &(globals->out_header_struct), SECS_PER_BLOCK,
                                        chan_names[chan_idx], NULL, /*globals->anonymize_output ? NULL : "pass",*/
                                        0 /*bit_shift_flag*/, str_temp /*write_thread_info->mef_path_ptr*/, i);
#endif
#ifdef OUTPUT_TO_MEF3
            initialize_mef_channel_data(globals->channel_state_struct[i],
                                        SECS_PER_BLOCK,           // seconds per block
                                        chan_names[chan_idx]  , // channel name
                                        0,// bit shift flag, set to 1 for neuralynx, to chop off 2 least-significant sample bits
                                        (lpf_value > 0.0) ? 0.0 : -1.0,           // low filt freq
                                        lpf_value,        // high filt freq
                                        -1.0,           // notch filt freq
                                        60.0,          // AC line freq
                                        globals->header_info.microvolts_per_AD_unit,           // units conversion factor
                                        "not_entered ",// chan description
                                        32000, // starter freq for channel, make it as high or higher than actual freq to allocate buffers
                                        SECS_PER_BLOCK * 1000000, // block interval, needs to be correct, this value is used for all channels
                                        chan_idx + 1,             // chan number
                                        str_temp,      // absolute path of session
                                        -6.0,                  // GMT offset
                                        "not_entered",        // session description
                                        "not_entered",                // anonymized subject name
                                        globals->anonymize_output ? "not_entered" : globals->header_info.pt_middlename,         // subject first name
                                        globals->anonymize_output ? "not_entered" : globals->header_info.pt_surname,                 // subject second name
                                        globals->anonymize_output ? "0-000-000" : globals->header_info.pt_id,               // subject ID
                                        globals->anonymize_output ? "not_entered" : "Mayo Clinic, Rochester, MN, USA",           // institution
                                        NULL,  // for now unencrypted
                                        NULL,  // for now unencrypted
                                        //globals->anonymize_output ? NULL : "pass",                  // level 1 password (technical data)
                                        //globals->anonymize_output ? NULL : "pass",               // level 2 password (subject data), must also specify level 1 password if specifying level 2
                                        "not_entered",        // study comments
                                        "not_entered",         // channel comments
                                        0                      // secs per segment, 0 means no limit to segment size
                                        );
            globals->proto_metadata_fps = globals->channel_state_struct[i]->metadata_fps;
#endif
        }
    }
    
    // Write montage file, if appropriate
    
    if (globals->montage_array != NULL)
    {
     
        if (globals->output_directory == NULL)
        {
#ifdef OUTPUT_TO_MEF2
            sprintf(str_temp, "mef21/generated.montage");
#endif
#ifdef OUTPUT_TO_MEF3
            sprintf(str_temp, "mef3.mefd/generated.montage");
#endif
        }
        else
        {
#ifdef OUTPUT_TO_MEF2
            sprintf(str_temp, "%s/generated.montage", globals->output_directory);
#endif
#ifdef OUTPUT_TO_MEF3
            sprintf(str_temp, "%s.mefd/generated.montage", globals->output_directory);
#endif
        }
        montage_file = (FILE *) fopen(str_temp, "wb+");
        
        for (i=0;i<MONTAGE_MAX_CHANS;i++)
        {
            // print space between names
            //if ((i > 0) && (globals->montage_array[i-1] != NULL))
            //    fprintf(stderr, " ");
            
            // print names
            if (globals->montage_array[i] != NULL)
            {
                //if (globals->skipping_names[i] == 0)
                    fprintf(montage_file, "%d\t%s\n", i+1, globals->montage_array[i]);
            }
            //else
            //    break;
        }
        
        fclose(montage_file);
    }

    
    
    
    /*
	dfps = globals->dfps = (FILE **) calloc((size_t) num_recorded_chans, sizeof(FILE *));
	tfps = globals->tfps = (FILE **) calloc((size_t) num_recorded_chans, sizeof(FILE *));
	if (dfps == NULL || tfps == NULL) { (void) fprintf(stderr, "\n\nInsufficient memory while allocating space for output data [%s ln %d]\n\n",__FILE__, __LINE__); exit(1); }
	//printf("globals->dfps %p\t globals->tfps %p\n", globals->dfps, globals->tfps);

	rec_to_phys_chan_map = globals->header_info.rec_to_phys_chan_map;
	chan_names = globals->chan_names;
	mef_header = globals->mef_header;
	for (i = 0; i < num_recorded_chans; ++i) {
		if (rec_to_phys_chan_map[i]) {
			chan_idx = rec_to_phys_chan_map[i] - 1;
			
			(void) sprintf(str_temp, "XLTek_single_channels/%s.mef", chan_names[chan_idx]);
			dfps[i] = (FILE *) fopen(str_temp, "w+");
			
			(void) sprintf(str_temp, "XLTek_single_channels/%s.mtf", chan_names[chan_idx]);
			tfps[i] = (FILE *) fopen(str_temp, "w+");
			
			if (dfps[i] == NULL || tfps[i] == NULL) { (void) fprintf(stderr, "\n\nCould not create the output file %s\n\n", str_temp); exit(1); }
			
			fseek(dfps[i], (size_t) MEF_HEADER_LENGTH, SEEK_SET);   // create space for headers, but write them later
			fseek(tfps[i], (size_t) MEF_HEADER_LENGTH, SEEK_SET);
		}
	}
	
	// Open event file
	(void) sprintf(str_temp, "XLTek_single_channels/%s_%s.mvf", mef_header + PATIENT_LAST_NAME_OFFSET, mef_header + PATIENT_ID_OFFSET);
	globals->efp = (FILE *) fopen(str_temp, "w+");
	fseek(globals->efp, (size_t) MEF_HEADER_LENGTH, SEEK_SET);   // create space for headers, but write them later
     
     */
    
#ifdef OUTPUT_EVENTS
    
    if (globals->output_directory == NULL)
    {
#ifdef OUTPUT_TO_MEF2
        sprintf(str_temp, "mef21/events.csv");
#endif
#ifdef OUTPUT_TO_MEF3
        sprintf(str_temp, "mef3.mefd/events.csv");
#endif
    }
    else
    {
#ifdef OUTPUT_TO_MEF2
        sprintf(str_temp, "%s/events.csv", globals->output_directory);
#endif
#ifdef OUTPUT_TO_MEF3
        sprintf(str_temp, "%s.mefd/events.csv", globals->output_directory);
#endif
    }
    globals->efp = (FILE *) fopen(str_temp, "wb+");
    
#endif
    
	
	return;
}

/*************************************************************** Fill in basic header info *************************************************************/

void	fill_basic_header_info(GLOBALS *globals)
{
	si4		i, l;
	si1		*mef_header, **chan_comments, c_resp[16];
	HEADER_INFO	*hi;
	//si2		rev_si2();
    //sf8		rev_sf8();
    sf8  temp_sf8;
    sf8 sampling_frequency;
    sf8 voltage_conversion_factor;
	
    si1 default_first_name[128];
    si1 default_third_name[128];
    si1 default_id[128];
    
    fprintf(stderr, "\n");
    
    hi = &globals->header_info;
    
    
    //sampling_frequency = rev_sf8(globals->samp_freq);
    sampling_frequency = globals->samp_freq;
    fprintf(stderr, "sampling_frequency = %f\n", sampling_frequency);
    
    //voltage_conversion_factor = rev_sf8(hi->microvolts_per_AD_unit);
    voltage_conversion_factor = hi->microvolts_per_AD_unit;
    fprintf(stderr, "voltage_conversion_factor = %f\n", voltage_conversion_factor);
    
    fprintf(stderr, "Patient first name [%s]: ", hi->pt_middlename);
    if (globals->noprompt)
    {
        sprintf(default_first_name, "");
        fprintf(stderr, "\n");
    }
    else
        fgets(default_first_name, 128, stdin);
    l = strlen(default_first_name) - 1;
    if (!l) strcpy(default_first_name, hi->pt_middlename);
    
    fprintf(stderr, "Patient last name [%s]: ", hi->pt_surname);
    if (globals->noprompt)
    {
        sprintf(default_third_name, "");
        fprintf(stderr, "\n");
    }
    else
        fgets(default_third_name, 128, stdin);
    l = strlen(default_third_name) - 1;
    if (!l) strcpy(default_third_name, hi->pt_surname);
    
    fprintf(stderr, "Patient ID [%s]: ", hi->pt_id);
    if (globals->noprompt)
    {
        sprintf(default_id, "");
        fprintf(stderr, "\n");
    }
    else
        fgets(default_id, 128, stdin);
    l = strlen(default_id) - 1;
    if (!l) strcpy(default_id, hi->pt_id);
    
    // study comments and channel comments not done, for now
    
#ifdef OUTPUT_TO_MEF2
    pack_mef_header(&(globals->out_header_struct), SECS_PER_BLOCK, NULL/*globals->anonymize_output ? NULL : "sieve"*/ /*session_password*/,
                    /*globals->anonymize_output ? NULL : "password"*/ /*subject_password*/ NULL,  0 /*uid*/,
                    0 /*anonymize_flag*/, 0 /*dst_flag*/, 0 /*bit_shift_flag*/, sampling_frequency, default_first_name, default_third_name, default_id,
                    voltage_conversion_factor);
#endif
    
    
    /*
	mef_header = globals->mef_header;
	hi = &globals->header_info;

	mef_header[BYTE_ORDER_CODE_OFFSET] = 0;     // Big-endian
	mef_header[HEADER_MAJOR_VERSION_OFFSET] = 1;
	mef_header[HEADER_MINOR_VERSION_OFFSET] = 0;
	mef_header[DELTA_BYTES_OFFSET] = 1;
	
	// ***** to here *****
	*((si2 *) (mef_header + KEY_SAMPLE_FLAG_OFFSET)) = rev_si2((si2) -128);
	*((si2 *) (mef_header + MAX_DELTA_OFFSET)) = rev_si2((si2) 127);
	*((si2 *) (mef_header + MIN_DELTA_OFFSET)) = rev_si2((si2) -127);
	temp_sf8 = rev_sf8(globals->samp_freq);
	memcpy(mef_header + SAMPLING_FREQUENCY_OFFSET, (ui1 *) &temp_sf8, 8);
	globals->key_samp_interval = (sf8) 1000.0 / globals->samp_freq;      // XLTek puts an ".etc" entry every 1000 samples - use this to trigger key samples and time alignment entries
	temp_sf8 = rev_sf8(globals->key_samp_interval);
	memcpy(mef_header + MAX_KEY_SAMPLE_INTERVAL_OFFSET, (ui1 *) &temp_sf8, 8);
	temp_sf8 = rev_sf8(globals->key_samp_interval);
	memcpy(mef_header + MAX_TIME_ALIGNMENT_INTERVAL_OFFSET, (ui1 *) &temp_sf8, 8);    // use same as key-sample interval for XLTek files
	temp_sf8 = rev_sf8(hi->microvolts_per_AD_unit);
	memcpy(mef_header + VOLTAGE_CONVERSION_FACTOR_OFFSET, (ui1 *) &temp_sf8, 8);
	strcpy(mef_header + ACQUISITION_SYSTEM_OFFSET, "XLTek EEG 128");
	
	// XLTek seems to switch firstname and middlename fields so switched here
	fprintf(stderr, "Patient first name [%s]: ", hi->pt_middlename); 
	fgets(mef_header + PATIENT_FIRST_NAME_OFFSET, PATIENT_FIRST_NAME_LENGTH, stdin);
	l = strlen(mef_header + PATIENT_FIRST_NAME_OFFSET) - 1; 
	*(mef_header + PATIENT_FIRST_NAME_OFFSET + l) = 0;
	if (!l) strcpy(mef_header + PATIENT_FIRST_NAME_OFFSET, hi->pt_middlename);

	fprintf(stderr, "Patient middle name [%s]: ", hi->pt_firstname); 
	fgets(mef_header + PATIENT_MIDDLE_NAME_OFFSET, PATIENT_MIDDLE_NAME_LENGTH, stdin);	
	l = strlen(mef_header + PATIENT_MIDDLE_NAME_OFFSET) - 1; 
	*(mef_header + PATIENT_MIDDLE_NAME_OFFSET + l) = 0;
	if (!l) strcpy(mef_header + PATIENT_MIDDLE_NAME_OFFSET, hi->pt_firstname);

	fprintf(stderr, "Patient last name [%s]: ", hi->pt_surname); 
	fgets(mef_header + PATIENT_LAST_NAME_OFFSET, PATIENT_LAST_NAME_LENGTH, stdin);
	l = strlen(mef_header + PATIENT_LAST_NAME_OFFSET) - 1; 
	*(mef_header + PATIENT_LAST_NAME_OFFSET + l) = 0;
	if (!l) strcpy(mef_header + PATIENT_LAST_NAME_OFFSET, hi->pt_surname);

	fprintf(stderr, "Patient ID [%s]: ", hi->pt_id); 
	fgets((si1 *) mef_header + PATIENT_ID_OFFSET, PATIENT_ID_LENGTH, stdin);
	l = strlen(mef_header + PATIENT_ID_OFFSET) - 1; 
	*(mef_header + PATIENT_ID_OFFSET + l) = 0;
	if (!l) strcpy(mef_header + PATIENT_ID_OFFSET, hi->pt_id);
	
	fprintf(stderr, "Study Comments: "); 
	fgets((si1 *) mef_header + STUDY_COMMENTS_OFFSET, STUDY_COMMENTS_LENGTH, stdin);	
	l = strlen(mef_header + STUDY_COMMENTS_OFFSET) - 1; 
	*(mef_header + STUDY_COMMENTS_OFFSET + l) = 0;
	
	fprintf(stderr, "Individual Channel Comments? (y/[n]): ");
	fgets((si1 *) c_resp, 16, stdin);	
	l = strlen(c_resp) - 1; 
	*(c_resp + l) = 0;
	if (c_resp[0] == 'y') {
		chan_comments = globals->chan_comments = (si1 **) calloc((size_t) hi->num_recorded_chans, sizeof(si1 *));
		//printf("globals->chan_comments %p\n", globals->chan_comments);

		for (i = 0; i < hi->num_recorded_chans; ++i) {
			if (!hi->rec_to_phys_chan_map[i]) continue;
			chan_comments[i] = (si1 *) calloc((size_t) CHANNEL_COMMENTS_LENGTH, sizeof(si1));
			//printf("chan_comments[%d] %p\n", i, chan_comments[i]);
			
			fprintf(stderr, "Comments for channel %d (%s): ", i, globals->chan_names[i]);
			fgets(chan_comments[i], CHANNEL_COMMENTS_LENGTH, stdin);
			l = strlen(chan_comments[i]) - 1;
			*(chan_comments[i] + l) = 0;
		}
	} else {
		globals->chan_comments = NULL;
	}
     */
    
    
	
	return;
}

/************************************************ Get data file list for this directory and sort by time **********************************************/

void	get_data_file_list(GLOBALS *globals, si4 dir_idx)
{
	si4		i, num_files, file_time, fid, len;
	si1		*tp, *t, command[1024];
    si1 str_temp[1024];
	FILE		*fp;
	ui8		flen, nr;
	FILE_TIME	*data_file_names;
	int		compare_times();
#ifndef _WIN32
	struct stat	sb;
#else
    struct _stat64	sb;
#endif
    si4  *data_file_samplestamps;
    si4 samplestamp_temp;
    si1** returned_list;
	
    /*

	sprintf(str_temp, get_random_tmp_filename());
	(void) sprintf(command, "ls \"%s/%s/\"*.erd > %s", globals->dir_list[dir_idx].path, globals->dir_list[dir_idx].name, str_temp);
	system(command);
	 
	fp = fopen(str_temp, "r");
	if (fp == NULL) { (void) fprintf(stderr, "\n\nCould not create the file \"./temp_file_paths\"\nExiting.\n"); exit(1); }
	
#ifndef _WIN32
	fid = fileno(fp);
    fstat(fid, &sb);
#else
    fid = _fileno(fp);
    _fstat64(fid, &sb);
#endif
	flen = sb.st_size;
	 
	t = (si1 *) malloc(flen);
	if (t == NULL) { (void) fprintf(stderr, "\n\nInsufficient memory while allocating t [%s %s ln %d]\nExiting.\n",__FILE__, __FUNCTION__, __LINE__); exit(1); }
	 
	nr = fread(t, sizeof(si1), (size_t) flen, fp);
	if (nr != flen) { (void) fprintf(stderr, "\n\nError reading the file \"./temp_file_paths\"\n\Exiting.n"); exit(1); }
	fclose(fp);
	//system("rm temp_file_paths");
	 
	for (num_files = i = 0; i < flen; ++i) 
		if (t[i] == '\n') {
			t[i] = 0;
			++num_files;
		}
	globals->num_files = num_files;
	 
	data_file_names = globals->data_file_names = (FILE_TIME *) calloc((size_t) num_files, sizeof(FILE_TIME));
	//printf("globals->data_file_names %p\n", globals->data_file_names);

	if (data_file_names == NULL) { (void) fprintf(stderr, "\n\nInsufficient memory while allocating data file names [%s ln %d]\nExiting.\n",__FILE__, __LINE__); exit(1); }
	 
	for (tp = t, i = 0; i < num_files; ++i) {
		data_file_names[i].name = tp;
		while (*tp++ != 0);
	}

    */

    // Use MEF 3 library to do the above operations:
    sprintf(command, "%s/%s/", globals->dir_list[dir_idx].path, globals->dir_list[dir_idx].name);
    returned_list = generate_file_list(NULL, &num_files, command, "erd");
    globals->num_files = num_files;
    data_file_names = globals->data_file_names = (FILE_TIME*)calloc((size_t)num_files, sizeof(FILE_TIME));
	 
	for (i = 0; i < num_files; ++i) {
        globals->data_file_names[i].name = data_file_names[i].name = returned_list[i];
		fp = fopen(data_file_names[i].name, "rb");
		if (fp == NULL) { (void) fprintf(stderr, "\n\nCould not open the file %s\nExiting.\n", data_file_names[i].name); exit(1); }
		
		fseek(fp, CREATION_TIME_OFFSET, SEEK_SET);
		nr = fread(&file_time, sizeof(si4), (size_t) 1, fp);
		if (nr != 1) { (void) fprintf(stderr, "\n\nCould not read the file %s\n\Exiting.n", data_file_names[i].name); exit(1); }
		fclose(fp);
		
		data_file_names[i].time = (ui8) file_time;
        
        // read samplestamp (4 bytes starting at offset 356) from associated .etc file.
        // the problem with file creation times, is sometimes multiple .erd files have identical times... ie, xltek creates multiple
        // files at the same time, with some of them "placeholders" which are filled in later.  But extracting the first samplestamp
        // from the associated .etc file seems more reliable, and this corresponds to the correct ordering.  Another equivalent way
        // getting the ordering is to read the .stc file and pull out names, which are (I think) in order correctly.  -DPC
        data_file_names[i].samplestamp = -1;
        
        strcpy(str_temp, data_file_names[i].name);
        len = strlen(str_temp) - 3;
        str_temp[len++] = 'e'; str_temp[len++] = 't'; str_temp[len++] = 'c';
        //fprintf(stderr, "reading file: %s\n", str_temp);
        fp = fopen(str_temp, "rb");
        if (fp == NULL) { (void) fprintf(stderr, "\n\nCould not open the file %s, skipping that segment.\n", str_temp); continue; }
        
        fseek(fp, SAMPLESTAMP_OFFSET , SEEK_SET);
        nr = fread(&samplestamp_temp, sizeof(si4), (size_t) 1, fp);
        if (nr != 1) { fprintf(stderr, "\n\nCould not read the file %s, skipping that segment.\n", str_temp); continue; }
        
        data_file_names[i].samplestamp = samplestamp_temp;
        
	}
    //qsort((void *) data_file_names, (size_t) num_files, sizeof(FILE_TIME), compare_times);  // unreliable, see note above
	qsort((void *) data_file_names, (size_t) num_files, sizeof(FILE_TIME), compare_samplestamps);
	 
	return;
}

/*************************************************** Read in the sync file entries for this directory ************************************************/

void	read_sync_entries(GLOBALS *globals, si4 dir_idx)
{
	si4		i, len, n_sync_recs, fid;
	si1		str_temp[1024];
	ui8		flen, nr, long_file_time, time_correction_factor;
	SYNC_INFO	*sync_info;
	FILE		*fp;
#ifndef _WIN32
	struct stat	sb;
#else
    struct _stat64	sb;
#endif


	sprintf(str_temp, "%s/%s/%s.snc", globals->dir_list[dir_idx].path, globals->dir_list[dir_idx].name, globals->dir_list[dir_idx].name);
	len = strlen(str_temp) - 8;
	if (!strncmp(str_temp + len, ".eeg", 4)) { 
		str_temp[len] = 0; 
		strcat(str_temp, ".snc");
	}
	
	fp = fopen(str_temp, "rb");
	if (fp == NULL) { (void) fprintf(stderr, "\n\nCould not open the file %s\nExiting.\n", str_temp); exit(1); }
	
#ifndef _WIN32
    fid = fileno(fp);
    fstat(fid, &sb);
#else
    fid = _fileno(fp);
    _fstat64(fid, &sb);
#endif
	flen = sb.st_size;
	fseek(fp, GENERIC_HEADER_END_OFFSET, SEEK_SET);
	
	n_sync_recs = globals->n_sync_recs = ((flen - GENERIC_HEADER_END_OFFSET) / 12) + 1;	// create 2 extra sync rec with samp_stamp 0
	sync_info = globals->sync_info = (SYNC_INFO *) calloc((size_t) n_sync_recs + 1, sizeof(SYNC_INFO)); // another extra at end, with value infinity for look ahead errors
	if (sync_info == NULL) { (void) fprintf(stderr, "\n\nInsufficient memory while allocating sync info [%s ln %d]\nExiting.\n",__FILE__, __LINE__); exit(1); }
	//printf("globals->sync_info %p\n", globals->sync_info);
	
	time_correction_factor = XLTEK_TIME_CORRECTION_FACTOR * 1000000;
	for (i = 1; i < n_sync_recs; ++i) {
		nr = fread(&sync_info[i].samp_stamp, sizeof(si4), (size_t) 1, fp);
		if (nr != 1) { 
            printf("Error reading sync file %s\nExiting.\n", str_temp); 
            exit(1); 
        }
		
		nr = fread(&long_file_time, sizeof(si8), (size_t) 1, fp);
		if (nr != 1) { 
            printf("Error reading sync file %s\nExiting.\n", str_temp); 
            exit(1); 
        }
		
		long_file_time = (long_file_time + 5) / 10;	
		long_file_time -= time_correction_factor;     // convert to Mayo time;
		
		sync_info[i].samp_time = long_file_time;
	}
	fclose(fp);
	
	sync_info[0].samp_stamp = 0;     // create 1 extra sync rec with samp_stamp 0
	sync_info[0].samp_time = sync_info[1].samp_time - (ui8) (((sf8) sync_info[1].samp_stamp * globals->samp_delta) + (sf8) 0.5);
	sync_info[n_sync_recs].samp_stamp = 0x0FFFFFFF;     // create 1 extra sync rec with samp_stamp infinity for look ahead errors
	sync_info[n_sync_recs].samp_time = 0xFFFFFFFFFFFFFFFF;
		
	return;
}

/********************************************************** Read ".etc" file for ".erd" file *********************************************************/

void	read_etc_file(GLOBALS *globals, si4 file_idx)
{
	si4		i, len, n_etc_entries, samp_nums[2], fid;
	si1		str_temp[1024];
	si2		etc_schema;
	ui8		nr, flen;
	ETC_INFO	*etc_info;
	FILE		*fp;
#ifndef _WIN32
	struct stat	sb;
#else
    struct _stat64	sb;
#endif
	
	strcpy(str_temp, globals->data_file_names[file_idx].name);
	len = strlen(str_temp) - 3; 
	str_temp[len++] = 'e'; str_temp[len++] = 't'; str_temp[len++] = 'c';
fprintf(stderr, "reading file: %s\n", str_temp);	
	fp = fopen(str_temp, "rb");
	if (fp == NULL) { (void) fprintf(stderr, "\n\nCould not open the file %s\nExiting.\n", str_temp); exit(1); }
	
	fseek(fp, FILE_SCHEMA_OFFSET, SEEK_SET);
	nr = fread(&etc_schema, sizeof(si2), (size_t) 1, fp);
	if (nr != 1) { 
        fprintf(stderr, "\n\nError reading sync file %s\nExiting.\n", str_temp); 
        exit(1); 
    }
	
	if (etc_schema != 3) { 
		(void) fprintf(stderr, "\n\nUnsupported schema in file %s\n\n", str_temp); 
		if (override_flag == 0) exit(1); 
	}
	
#ifndef _WIN32
    fid = fileno(fp);
    fstat(fid, &sb);
#else
    fid = _fileno(fp);
    _fstat64(fid, &sb);
#endif
	flen = sb.st_size;
	n_etc_entries = globals->n_etc_entries = (flen - GENERIC_HEADER_END_OFFSET) / 16;
	
	fseek(fp, GENERIC_HEADER_END_OFFSET - 4, SEEK_SET);
	etc_info = globals->etc_info = (ETC_INFO *) calloc((size_t) n_etc_entries, sizeof(ETC_INFO));
	//printf("globals->etc_info %p\n", globals->etc_info);
	for (i = 0; i < n_etc_entries; ++i) {
		fseek(fp, 8, SEEK_CUR);
		
		nr = fread(samp_nums, sizeof(si4), (size_t) 2, fp);
		if (nr != 2) { 
            printf("Error reading etc file %s\nExiting.\n", str_temp); 
            exit(1); 
        }
		
		etc_info[i].samp_stamp = samp_nums[0];
		etc_info[i].samp_num = samp_nums[1];
	}				
	fclose(fp);
		
	return;
}

/************************************* Convert sample stamps to Mayo time using sync info & align with true sample numbers *************************/

si4	make_mayo_sync_info(GLOBALS *globals)
{
	si4		i, n_mayo_sync_entries, curr_sync_idx;
	si1		*time_str;
	sf8		samp_delta;
	si8		long_file_time;
	time_t		temp_time;
	MAYO_SYNC_INFO	*mayo_sync_info;
	ETC_INFO	*etc_info;
	SYNC_INFO	*sync_info;
	
	n_mayo_sync_entries = globals->n_mayo_sync_entries = globals->n_etc_entries + 1;
	mayo_sync_info = globals->mayo_sync_info = (MAYO_SYNC_INFO *) calloc((size_t) n_mayo_sync_entries + 1, sizeof(MAYO_SYNC_INFO));
	if (mayo_sync_info == NULL) { (void) fprintf(stderr, "\n\nInsufficient memory while allocating mayo_sync_info [%s ln %d]\nExiting.\n",__FILE__, __LINE__); exit(1); }

	curr_sync_idx = globals->curr_sync_idx;
	samp_delta= globals->samp_delta;
	etc_info = globals->etc_info;
	sync_info = globals->sync_info;
	
	for (i = 0; i < globals->n_etc_entries; ++i) {
		if (etc_info[i].samp_stamp >= sync_info[curr_sync_idx + 1].samp_stamp)
			++curr_sync_idx;
		mayo_sync_info[i].samp_num = etc_info[i].samp_num;
		mayo_sync_info[i].mayo_time = sync_info[curr_sync_idx].samp_time + (ui8) (((sf8) (etc_info[i].samp_stamp - sync_info[curr_sync_idx].samp_stamp) * samp_delta) + (sf8) 0.5);	
	}
	mayo_sync_info[n_mayo_sync_entries - 1].samp_num = 0x0FFFFFFF;
	
	if (mayo_sync_info[0].mayo_time < globals->last_sync_entry) { 
		long_file_time = (mayo_sync_info[0].mayo_time + 500000) / 1000000;
		temp_time = (time_t) long_file_time;
		time_str = ctime(&temp_time);
		time_str[24] = 0;
		(void) fprintf(stderr, "\nSync entry overlap: %s < ", time_str);
		long_file_time = (globals->last_sync_entry + 500000) / 1000000;
		temp_time = (time_t) long_file_time;
		time_str = ctime(&temp_time);
		time_str[24] = 0;
		(void) fprintf(stderr, "%s\n", time_str);
		return(1); 
	}
	if (mayo_sync_info[0].mayo_time > globals->current_time) { 
		long_file_time = (mayo_sync_info[0].mayo_time + 500000) / 1000000;
		temp_time = (time_t) long_file_time;
		time_str = ctime(&temp_time);
		time_str[24] = 0;
		(void) fprintf(stderr, "\nSync entry error: %s > ", time_str);
		long_file_time = (globals->current_time + 500000) / 1000000;
		temp_time = (time_t) long_file_time;
		time_str = ctime(&temp_time);
		time_str[24] = 0;
		(void) fprintf(stderr, "%s\n", time_str);
		return(1); 
	}
    
    // debug code
    //long_file_time = (globals->last_sync_entry + 500000) / 1000000;
    //temp_time = (time_t) long_file_time;
    //time_str = ctime(&temp_time);
    //time_str[24] = 0;
    //(void) fprintf(stderr, "Previous last entry: %s\n", time_str);
  
    
    
	globals->last_sync_entry = mayo_sync_info[n_mayo_sync_entries - 2].mayo_time;

	globals->curr_sync_idx = curr_sync_idx;
		
	return(0);
}

/************************************************************** read data (".erd") file into memory ************************************************/

void	read_data_file(GLOBALS *globals, si4 file_idx)
{
	si4		fid;
	FILE		*fp;
	ui8		nr;
#ifndef _WIN32
	struct stat	sb;
#else
    struct _stat64	sb;
#endif
	
	fp = fopen(globals->data_file_names[file_idx].name, "rb");
	if (fp == NULL) { (void) fprintf(stderr, "\n\nCould not open the file %s\nExiting.\n", globals->data_file_names[file_idx].name); exit(1); }
	
#ifndef _WIN32
    fid = fileno(fp);
    fstat(fid, &sb);
#else
    fid = _fileno(fp);
    _fstat64(fid, &sb);
#endif
	globals->flen = sb.st_size;

	globals->data = (si1 *) malloc(globals->flen + (globals->header_info.num_recorded_chans * 6));  // pad for interrupted terminal packet
	if (globals->data == NULL) { (void) fprintf(stderr, "\n\nInsufficient memory  while allocating globals->data [%s ln %d]\nExiting.\n",__FILE__, __LINE__); exit(1); }
	
	nr = fread(globals->data, sizeof(si1), globals->flen, fp); 
	if (nr != globals->flen) { 
        (void) fprintf(stderr, "\n\nError reading the file %s\nExiting.\n", globals->data_file_names[file_idx].name); 
        exit(1); 
    }
	fclose(fp);
	
	return;
}

/************************************************************** read and compare header info *******************************************************/

void		compare_headers(GLOBALS *globals, HEADER_INFO *hi, HEADER_INFO *hin)
{
	si4	i, chan_idx;
	void	compare_header_info();

	read_XLTek_header(globals->data, hin, globals->anonymize_output);
	
	for (i = 0; i < hin->num_recorded_chans; ++i) { 
		chan_idx = hin->rec_to_phys_chan_map[i] - 1; 
		if (!globals->chan_names[chan_idx][0]) 
			hin->rec_to_phys_chan_map[i] = 0; 
	}
	
	compare_header_info(hi, hin);
	
	return;
}

/****************************************************************** extract samples *****************************************************************/

void		read_data(GLOBALS *globals, HEADER_INFO *hi)
{
	si1	*cdp, *hb, *data_end;
	si1	*overflow_chans, *delta_mask, mask_byte;
	si4	i, *chan_sums, overflows, **channel_data, delta_mask_len, num_recorded_chans, *rec_to_phys_chan_map; 
	ui8	num_samps, max_samps_per_chan; 
	si2	short_delta;
    si4 mask_bit_counter;

	num_recorded_chans = hi->num_recorded_chans;
	rec_to_phys_chan_map = hi->rec_to_phys_chan_map;
	cdp = globals->data + SAMPLE_PACKET_OFFSET;
	data_end = globals->data + globals->flen;
	delta_mask_len = (si4) ceil((sf8) hi->num_system_chans / (sf8) 8.0);
	
	overflow_chans = (si1 *) malloc((size_t) num_recorded_chans);
	chan_sums = (si4 *) calloc((size_t) num_recorded_chans, sizeof(si4)*2);
	//printf("chan_sums %p\n", chan_sums);
	if (overflow_chans == NULL || chan_sums == NULL) { (void) fprintf(stderr, "\n\nInsufficient memory while allocating overflow and chan_sums [%s ln %d]\nExiting.\n",__FILE__, __LINE__); exit(1); }
	
	max_samps_per_chan = (ui8) ceil((double) (globals->flen - SAMPLE_PACKET_OFFSET) / (double) (1 + delta_mask_len + num_recorded_chans));
	channel_data = globals->channel_data = (si4 **) calloc((size_t) num_recorded_chans, sizeof(si4 *));
	//printf("channel_data %p\n", channel_data);

	if (channel_data == NULL) { (void) fprintf(stderr, "\n\nInsufficient memory while allocating channel data [%s ln %d]\nExiting.\n",__FILE__, __LINE__); exit(1); }
	for (i = 0; i < num_recorded_chans; ++i) {
		if (rec_to_phys_chan_map[i]) {
			channel_data[i] = (si4 *) calloc((size_t) max_samps_per_chan, sizeof(si4));
			if (channel_data[i] == NULL) { (void) fprintf(stderr, "\n\nInsufficient memory while allocating channel data [%s ln %d]\nExiting.\n",__FILE__, __LINE__); exit(1); }
		}
	}
 	// data reading loop
	num_samps = 0;
	while (cdp < data_end) {
		/* if (*cdp) {
			if (*cdp == 1)
				(void) fprintf(stderr, "\n\nPhotic\n\n"); 
			else
				(void) fprintf(stderr, "\n\nUnrecognized event byte (%d): update software\n\n", *((ui1 *) cdp));
		} */
		delta_mask = (si1 *) ++cdp;
		overflows = 0;
		mask_byte = 1;
		cdp += delta_mask_len;
        mask_bit_counter = 1;  // counting starts at 1 within rec_to_phys_chan_map array
		for (i = 0; i < num_recorded_chans; ++i) {
            // skip this mask bits that refer to shorted channels
            while (rec_to_phys_chan_map[i] != mask_bit_counter)
            {
                if (!(mask_byte <<= 1)) {
                    mask_byte = 1;
                    ++delta_mask;
                }
                mask_bit_counter++;
            }
			if (*delta_mask & mask_byte) {	
				hb = (si1 *) &short_delta;
				*hb++ = *cdp++; 
				*hb = *cdp++;
				if (short_delta == (si2) XLTEK_OVERFLOW_FLAG) { 
					overflow_chans[i] = 1; 
					overflows = 1; 
				} else {
					chan_sums[i] += (si4) short_delta; 
				}
			} else {
				chan_sums[i] += (si4) *cdp++; 
			}
			if (!(mask_byte <<= 1)) {
				mask_byte = 1; 
				++delta_mask;
			}
            mask_bit_counter++;
		}
		if (overflows) {
			for (i = 0; i < num_recorded_chans; ++i) {
				if (overflow_chans[i]) { 
					hb = (si1 *) (chan_sums + i); 
					*hb++ = *cdp++; 
					*hb++ = *cdp++; 
					*hb++ = *cdp++; 
					*hb = *cdp++;
					overflow_chans[i] = 0;
				}
			}
	 	}
		for (i = 0; i < num_recorded_chans; ++i) {
			if (rec_to_phys_chan_map[i]) {
				//printf("i %d\t chan_sums[i] %d channel_data[i] %p", i, chan_sums[i], channel_data[i]);
				channel_data[i][num_samps] = chan_sums[i]; 
				//printf("\b\b\b\b\b\b\b\b\b");
				//printf("i %d\t channel_data[i][num_samps] %d channel_data[i] %p\n", i, channel_data[i][num_samps], channel_data[i]);
			}
		}
		++num_samps;
	}
	if (cdp > data_end)
		--num_samps;
	globals->file_num_samps = num_samps;
	
	free(overflow_chans); overflow_chans=NULL;
	free(chan_sums); chan_sums=NULL;
	
	return;
}

/****************************************************************** Write out in Mayo format *****************************************************************/

void		write_out(GLOBALS *globals, HEADER_INFO *hi)
{
	si4		i, j, mayo_sync_idx, *chan_arr, val_diff;
	si1		/* *out_chan, */ *ocp, *hb;
	ui8		/* *data_chan_offsets,*/ num_samps, /* *time_recs,  *trp,*/ items_to_write, tot_num_samps, nw, rev_ui8();
	FILE		**dfps, **tfps;
	MAYO_SYNC_INFO	*mayo_sync_info;
    ui8   timestamp_base;
    si4   samples_since_timestamp_base;
	
    PACKET_TIME packet_times[1];
    si4 samps[1];
    si1 *samp_value_ptr;
    
	//data_chan_offsets = globals->data_chan_offsets;
	dfps = globals->dfps;
	tfps = globals->tfps;
	num_samps = globals->file_num_samps;
	mayo_sync_info = globals->mayo_sync_info;
	tot_num_samps = globals->tot_num_samps;
	
	//out_chan = (si1 *) calloc((size_t) num_samps * 4, sizeof(si1));     // max 4 bytes per sample if every sample is key sample

	//time_recs = (ui8 *) calloc((size_t) 3* num_samps, sizeof(ui8));     // 3 ui8 entries for each time rec
	//if (time_recs == NULL || out_chan == NULL) { (void) fprintf(stderr, "\n\nInsufficient memory while allocating time recs and out chan [%s ln %d]\n\n",__FILE__, __LINE__); exit(1); }
	
	for (i = 0; i < hi->num_recorded_chans; ++i) {
		if (!hi->rec_to_phys_chan_map[i]) 
			continue;
        if (globals->channel_state_struct[i] == NULL)
            continue;
		mayo_sync_idx = 0;
		//ocp = out_chan;
		chan_arr = globals->channel_data[i];
		//trp = time_recs;
		
		// first sample is key sample and gets a time alignment record
		//*trp++ = rev_ui8(mayo_sync_info[0].mayo_time);     // time file sample time
		//*trp++ = rev_ui8((ui8) data_chan_offsets[i]);     // time file data offset
		//*trp++ = rev_ui8(globals->tot_num_samps);     // time file sample number
		//*ocp++ = -128;    // key sample flag
		//hb = ((si1 *) chan_arr) + 2;
        //samp_value_ptr = ocp;  // new for MEF
		//*ocp++ = *hb--; *ocp++ = *hb--; *ocp++ = *hb;    // full value as si3
        
     
        // MEF 2 writing
        packet_times[0].timestamp = mayo_sync_info[0].mayo_time;
        timestamp_base = mayo_sync_info[0].mayo_time;
        samps[0] = chan_arr[0];
        //if (i == 0)
        //    fprintf(stderr, "timestamp=%lld sample=%d\n", packet_times[0].timestamp, samps[0]);
#ifdef OUTPUT_TO_MEF2
        write_mef_channel_data(globals->channel_state_struct[i], &(globals->out_header_struct), packet_times, samps, 1, SECS_PER_BLOCK);
#endif
#ifdef OUTPUT_TO_MEF3
        write_mef_channel_data(globals->channel_state_struct[i], packet_times, samps, 1, SECS_PER_BLOCK, globals->samp_freq);
#endif
        samples_since_timestamp_base = 0;
		
		
		for (j = 1; j < num_samps; ++j)
        {
			val_diff = chan_arr[j] - chan_arr[j - 1];
			if (j == mayo_sync_info[mayo_sync_idx + 1].samp_num)
            {
				++mayo_sync_idx;
                
                // MEF 2 writing
                timestamp_base = mayo_sync_info[mayo_sync_idx].mayo_time;
				
				//fprintf(stderr, "new base: %lld\n", timestamp_base);
                samples_since_timestamp_base = 0;
                
				//*trp++ = rev_ui8(mayo_sync_info[mayo_sync_idx].mayo_time);     // time file sample time
				//*trp++ = rev_ui8(data_chan_offsets[i] + (ocp - out_chan));     // time file data offset
				//*trp++ = rev_ui8(tot_num_samps + j);     // time file sample number
				//val_diff = 128;    // force a key sample
			}
			if (val_diff > 127 || val_diff < -127)
            {     // need key sample
				//*ocp++ = -128;    // key sample flag
				//hb = ((si1 *) (chan_arr + j)) + 2;
				//*ocp++ = *hb--; *ocp++ = *hb--; *ocp++ = *hb;    // full value as si3
                
                // MEF writing
#ifdef OUTPUT_TO_MEF2
                packet_times[0].timestamp = timestamp_base + ((samples_since_timestamp_base * 1e6) / globals->out_header_struct.sampling_frequency);
#endif
#ifdef OUTPUT_TO_MEF3
                packet_times[0].timestamp = timestamp_base + ((samples_since_timestamp_base * 1e6) / globals->samp_freq);
#endif
                samps[0] = chan_arr[j];
                samples_since_timestamp_base++;
                
                //if (i == 0)
                //    fprintf(stderr, "1timestamp=%lld sample=%d rate = %f timestamp_base = %lld samples_since_timestamp_base = %d\n", packet_times[0].timestamp, samps[0], globals->out_header_struct.sampling_frequency, timestamp_base, samples_since_timestamp_base);
#ifdef OUTPUT_TO_MEF2
                write_mef_channel_data(globals->channel_state_struct[i], &(globals->out_header_struct), packet_times, samps, 1, SECS_PER_BLOCK);
#endif
#ifdef OUTPUT_TO_MEF3
                write_mef_channel_data(globals->channel_state_struct[i], packet_times, samps, 1, SECS_PER_BLOCK, globals->samp_freq);
#endif
                
                
			} else
            {
				//*ocp++ = (si1) val_diff;
                
                // MEF writing
#ifdef OUTPUT_TO_MEF2
                packet_times[0].timestamp = timestamp_base + ((samples_since_timestamp_base * 1e6) / globals->out_header_struct.sampling_frequency);
#endif
#ifdef OUTPUT_TO_MEF3
                packet_times[0].timestamp = timestamp_base + ((samples_since_timestamp_base * 1e6) / globals->samp_freq);
#endif
                samps[0] = chan_arr[j];
                samples_since_timestamp_base++;
                
                //if (i == 0)
                //    fprintf(stderr, "2timestamp=%lld sample=%d\n", packet_times[0].timestamp, samps[0]);
#ifdef OUTPUT_TO_MEF2
                write_mef_channel_data(globals->channel_state_struct[i], &(globals->out_header_struct), packet_times, samps, 1, SECS_PER_BLOCK);
#endif
#ifdef OUTPUT_TO_MEF3
                write_mef_channel_data(globals->channel_state_struct[i], packet_times, samps, 1, SECS_PER_BLOCK, globals->samp_freq);
#endif
			}
		}
		// write out compressed channel data
		//items_to_write = ocp - out_chan;
		//nw = fwrite(out_chan, sizeof(ui1), (size_t) items_to_write, dfps[i]);
		//if (nw != items_to_write) { (void) fprintf(stderr, "\n\nError writing eeg data for channel %d\n\n", hi->rec_to_phys_chan_map[i]); exit(1); }
		//data_chan_offsets[i] += items_to_write;
		
		// write out time alignment data
		//items_to_write = trp - time_recs;
		//nw = fwrite(time_recs, sizeof(ui8), (size_t) items_to_write, tfps[i]);
		//if (nw != items_to_write) { (void) fprintf(stderr, "\n\nError writing time alignment data for channel %d\n\n", hi->rec_to_phys_chan_map[i]); exit(1); }
	}
	
	globals->tot_num_samps += num_samps;
	globals->tot_time_recs += globals->n_etc_entries;
	
	//free(out_chan); out_chan = NULL;
	//free(time_recs); time_recs = NULL;
	
	return;
}

/******************************************************* Fill in remaining header info and close files ***********************************************************/

void	close_files(GLOBALS *globals, HEADER_INFO *hi)
{
	si4	i, rev_si4(), chan_num;
	si1	*mef_header;
	ui8	nrw, num_samps, rev_ui8();
	FILE	**dfps, **tfps;

    
    
	dfps = globals->dfps;
	tfps = globals->tfps;
	//mef_header = globals->mef_header;
	
    fprintf(stderr, "\nClosing MEF output files...\n");
    
    for (i=0;i<hi->num_recorded_chans;i++)
    {
        if (!hi->rec_to_phys_chan_map[i])
            continue;
        if (globals->channel_state_struct[i] == NULL)
            continue;
        
#ifdef OUTPUT_TO_MEF2
        // load channel number and channel name to MEF 2 header
        // these are (obviously) channel specific
        globals->out_header_struct.physical_channel_number = hi->rec_to_phys_chan_map[i];
        sprintf(globals->out_header_struct.channel_name, "%s", globals->chan_names[hi->rec_to_phys_chan_map[i] - 1]);

        // finish and close MEF 2 file write
        close_mef_channel_file(globals->channel_state_struct[i], &(globals->out_header_struct), "pass", "pass", SECS_PER_BLOCK);
#endif
#ifdef OUTPUT_TO_MEF3
        close_mef_channel(globals->channel_state_struct[i]);
#endif
        
    }
    
    fprintf(stderr, "Done.\n");
    
    
    /*
    
	// get recording start time (time of first sample)
	for (i = 0; i < hi->num_recorded_chans; ++i) 
		if (hi->rec_to_phys_chan_map[i]) 
			break;
	fseek(tfps[i], MEF_HEADER_LENGTH, SEEK_SET);
	nrw = fread(mef_header + RECORDING_START_TIME_OFFSET, sizeof(ui8), (size_t) 1, tfps[i]);
	if (nrw != 1) { (void) fprintf(stderr, "\n\nError reading channel %d\n\n", hi->rec_to_phys_chan_map[i]); exit(1); }
	*/
    
    /*
	for (i = 0; i < hi->num_recorded_chans; ++i) {
		
		if (!hi->rec_to_phys_chan_map[i])
			continue;
		bzero(mef_header + CHANNEL_NAME_OFFSET, CHANNEL_NAME_LENGTH);
		strcpy(mef_header + CHANNEL_NAME_OFFSET, globals->chan_names[hi->rec_to_phys_chan_map[i] - 1]);
		chan_num = rev_si4(hi->rec_to_phys_chan_map[i]);
		memcpy(mef_header + PHYSICAL_CHANNEL_NUMBER_OFFSET, (ui1 *) &chan_num, 4);
		bzero(mef_header + CHANNEL_COMMENTS_OFFSET, CHANNEL_COMMENTS_LENGTH);
		if (globals->chan_comments != NULL) strcpy(mef_header + CHANNEL_COMMENTS_OFFSET, globals->chan_comments[i]);
		
		// write ".mef" header
		strcpy(mef_header + FILE_TYPE_OFFSET, "mef");
		num_samps = globals->tot_num_samps;
		num_samps = rev_ui8(num_samps);
		memcpy(mef_header + NUMBER_OF_ENTRIES_OFFSET, (ui1 *) &num_samps, 8);		
		rewind(dfps[i]);
		nrw = fwrite(mef_header, sizeof(ui1), (size_t) MEF_HEADER_LENGTH, dfps[i]); 
		if (nrw != MEF_HEADER_LENGTH) { (void) fprintf(stderr, "\n\nError writing channel %d\n\n", hi->rec_to_phys_chan_map[i]); exit(1); }		
		fclose(dfps[i]);
		
		// write ".mtf" header
		strcpy(mef_header + FILE_TYPE_OFFSET, "mtf");
		num_samps = rev_ui8(globals->tot_time_recs);
		memcpy(mef_header + NUMBER_OF_ENTRIES_OFFSET, (ui1 *) &num_samps, 8);		
		rewind(tfps[i]);
		nrw = fwrite(mef_header, sizeof(ui1), (size_t) MEF_HEADER_LENGTH, tfps[i]); 
		if (nrw != MEF_HEADER_LENGTH) { (void) fprintf(stderr, "\n\nError writing channel %d\n\n", hi->rec_to_phys_chan_map[i]); exit(1); }
		fclose(globals->tfps[i]);
	}
     */

    /*
	// write ".mvf" header
	strcpy(mef_header + FILE_TYPE_OFFSET, "mvf");
	num_samps = rev_ui8(globals->tot_num_events);
	memcpy(mef_header + NUMBER_OF_ENTRIES_OFFSET, (ui1 *) &num_samps, 8);
	bzero(mef_header + CHANNEL_NAME_OFFSET, CHANNEL_NAME_LENGTH);
	bzero(mef_header + PHYSICAL_CHANNEL_NUMBER_OFFSET, 4);
	bzero(mef_header + CHANNEL_COMMENTS_OFFSET, CHANNEL_COMMENTS_LENGTH);
	rewind(globals->efp);
	nrw = fwrite(mef_header, sizeof(ui1), (size_t) MEF_HEADER_LENGTH, globals->efp); 
	if (nrw != MEF_HEADER_LENGTH) { (void) fprintf(stderr, "\n\nError writing event file header\n\n"); exit(1); }
	fclose(globals->efp);
	*/
    
	return;
}

/****************************************************************** Read XLTek Header *****************************************************************/	

void		read_XLTek_header(si1 *data, HEADER_INFO *header_info, si1 patient_anon)
{
	si4	i, j, err, supported_headbox_types[NUM_SUPPORTED_HEADBOX_TYPES] = SUPPORTED_HEADBOX_TYPES;
	si4	*phys_to_stor_chan_map, supported_schemata[NUM_SUPPORTED_SCHEMATA] = SUPPORTED_SCHEMATA;
	si2	*short_temp;
	si1	*cdp, *hb, *lb;
	ui1	raw_data_guid[GUID_BYTES] = RAW_DATA_GUID;
	
	
	/* read file guid */
	cdp = data + GUID_OFFSET;
//	printf("cdp %lu guid %lu\n", (ui8)(*cdp), (ui8)raw_data_guid);
	err = memcmp(cdp, raw_data_guid, (si8) GUID_BYTES);
	if (err) { (void) fprintf(stderr, "\n\nWrong data file type\nExiting.\n"); exit(1); }
	
	/* read file schema & reverse bytes */
	cdp = data + FILE_SCHEMA_OFFSET;
	header_info->file_schema = 0;
	hb = (si1 *) &(header_info->file_schema); lb = cdp;
	*hb++ = *lb++; *hb = *lb;
	for (i = 0; i < NUM_SUPPORTED_SCHEMATA; ++i) { if (header_info->file_schema == supported_schemata[i]) break; }
	if (i == NUM_SUPPORTED_SCHEMATA) { 
		(void) fprintf(stderr, "\n\nUnsupported file schema (%d) in the data file\n\n", header_info->file_schema);
		if (override_flag==0) exit(1); 
	}
	
	/* read file creation time, reverse bytes, and convert to 64-bit int */
	/* commented out, because it doesn't appear to be used anywhere
    cdp = data + CREATION_TIME_OFFSET;
	header_info->creation_time = 0;
	hb = (si1 *) &(header_info->creation_time); lb = cdp;
	for (i = 4; i--;) { *hb++ = *lb++; }
	(void) ctime_r(&(header_info->creation_time), header_info->time_string); header_info->time_string[24] = 0;
    */
	
    if (patient_anon)
    {
        strcpy(header_info->pt_surname, "not_entered");
        strcpy(header_info->pt_firstname, "not_entered");
        strcpy(header_info->pt_middlename, "not_entered");
        strcpy(header_info->pt_id, "not_entered");
    }
    else
    {
        /* read patient surname */
        cdp = data + PT_SURNAME_OFFSET;
        if (*cdp) { strcpy(header_info->pt_surname, cdp); }
        else { strcpy(header_info->pt_surname, "not_entered"); }
        
        /* read patient firstname */
        cdp = data + PT_FIRSTNAME_OFFSET;
        if (*cdp) { strcpy(header_info->pt_firstname, cdp); }
        else { strcpy(header_info->pt_firstname, "not_entered"); }
        
        /* read patient middlename */
        cdp = data + PT_MIDDLENAME_OFFSET;
        if (*cdp) { strcpy(header_info->pt_middlename, cdp); }
        else { strcpy(header_info->pt_middlename, "not_entered"); }
        
        /* read patient id */
        cdp = data + PT_ID_OFFSET;
        if (*cdp) { strcpy(header_info->pt_id, cdp); }
        else { strcpy(header_info->pt_id, "not_entered"); }
    }
	
	/* read sample frequency & reverse bytes */
	cdp = data + SAMP_FREQ_OFFSET;
	hb = (si1 *) &(header_info->samp_freq); lb = cdp;
	for (i = 8; i--;) { *hb++ = *lb++; }
	
	/* read num system chans & reverse bytes */
	cdp = data + NUM_CHANS_OFFSET;
	hb = (si1 *) &(header_info->num_system_chans); lb = cdp;
	for (i = 4; i--;) { *hb++ = *lb++; }
	
	/* read delta bits & reverse bytes */
	cdp = data + DELTA_BITS_OFFSET;
	hb = (si1 *) &(header_info->num_delta_bits); lb = cdp;
	for (i = 4; i--;) { *hb++ = *lb++; }
	if (header_info->num_delta_bits != SUPPORTED_DELTA_BITS) { (void) fprintf(stderr, "\n\nUnsupported delta bits in the data file\nExiting.\n"); exit(1); }
	
	/* read physical to stored channel map array and reverse  bytes */
	cdp = data + PHYS_TO_STOR_CHAN_MAP_OFFSET;
	phys_to_stor_chan_map = (int *) calloc((size_t) header_info->num_system_chans, sizeof(int));
	if (phys_to_stor_chan_map == NULL) { (void) fprintf(stderr, "\n\nInsufficient memory\nExiting.\n"); exit(1); }
	hb = (si1 *) phys_to_stor_chan_map; lb = cdp;
	for (i = header_info->num_system_chans; i--;) { for (j = 4; j--;) { *hb++ = *lb++; } }
	for (i = 0; i < header_info->num_system_chans; ++i) { if (phys_to_stor_chan_map[i] != i) { (void) fprintf(stderr, "\n\nUnsupported physical to storage map: update software\nExiting.\n"); exit(1); } }
	//free(phys_to_stor_chan_map); //NOTE: Freeing this ptr caused malloc free errors for globals.channel_data in main
									//TO DO: double check that this doesn't cause a memory leak!
	phys_to_stor_chan_map = NULL;
	
	/* read headbox type array and reverse bytes */
	cdp = data + HEADBOX_TYPE_ARRAY_OFFSET;
	hb = (si1 *) &(header_info->headbox_types); lb = cdp;
	for (i = HEADBOX_TYPE_ARRAY_SIZE; i--;) { for (j = 4; j--;) { *hb++ = *lb++; } }
	for (header_info->num_headboxes = i = 0; i < HEADBOX_TYPE_ARRAY_SIZE; ++i) { 
		if (header_info->headbox_types[i]) {
            fprintf(stderr, "headbox_type = %d  ", header_info->headbox_types[i]);
			++header_info->num_headboxes;
			for (j = 0; j < NUM_SUPPORTED_HEADBOX_TYPES; ++j) { if (header_info->headbox_types[i] == supported_headbox_types[j]) break; }
			//if (j == NUM_SUPPORTED_HEADBOX_TYPES) { (void) fprintf(stderr, "\n\nUnsupported headbox (type %d): update software\n\n", header_info->headbox_types[i]); /*exit(1);*/ }
		}
	}
	
	/* read discard bits & reverse bytes */
	cdp = data + DISCARD_BITS_OFFSET;
	hb = (si1 *) &(header_info->num_discard_bits); lb = cdp;
	for (i = 4; i--;) { *hb++ = *lb++; }
	header_info->microvolts_per_AD_unit = ((double) 8711 / ((double) ((unsigned int) 1 << 21) - (double) 0.5)) * (double) ((unsigned int) 1 << header_info->num_discard_bits);
	
    // based on 2015 (version 8.1 format spec)
    // 10/2021 - commenting this out, because it appears that Natus itself doesn't use this negative formula.  This was verified by Mayo internal testing, where
    // the same data pulled up in Neuroworks shows a postive conversion factor, depsite the negative number in the formula.  Biologically, it also made sense to
    // use the positive formula.

    //if (header_info->num_headboxes >= 1)
    //{
    //    if (header_info->headbox_types[0] == 20)  // Quantum headbox
    //        header_info->microvolts_per_AD_unit = -(header_info->microvolts_per_AD_unit);  // same formula, but uses -8711 instead of +8711
    //}
    
	/* read shorted channels & place in rec_to_phys_chan_map */
	/* this array will contain the physical channel number of the consecutive samples as recorded in sample packet order */
	header_info->rec_to_phys_chan_map = (si4 *) calloc((size_t) header_info->num_system_chans, sizeof(si4));
	short_temp = (si2 *) calloc((size_t) header_info->num_system_chans, sizeof(si2));
	if (header_info->rec_to_phys_chan_map == NULL || short_temp == NULL) { (void) fprintf(stderr, "\n\nInsufficient memory\nExiting.\n"); exit(1); }
	cdp = data + SHORTED_CHANS_OFFSET;
	(void) memcpy(short_temp, cdp, header_info->num_system_chans * sizeof(si2));
	header_info->num_recorded_chans = 0;
	for (i = 0; i < header_info->num_system_chans; ++i) { if (!short_temp[i]) { header_info->rec_to_phys_chan_map[header_info->num_recorded_chans++] = i + 1; } }
fprintf(stderr, "num_recorded_chans = %d\n", header_info->num_recorded_chans);
	free(short_temp); short_temp = NULL;
	
	/* read and check skip factors - if present will need to include reading frequency byte in sample packets */
	short_temp = (si2 *) calloc((size_t) header_info->num_system_chans, sizeof(si2));
	if (short_temp == NULL) { (void) fprintf(stderr, "\n\nInsufficient memory\nExiting.\n"); exit(1); }
	cdp = data + SKIP_FACT_OFFSET;
	(void) memcpy(short_temp, cdp, header_info->num_system_chans * sizeof(si2));
	for (i = 0; i < header_info->num_system_chans; ++i) { if (short_temp[i] != NO_SKIP) { (void) fprintf(stderr, "\n\nNon-zero value in skip factor array: update software\nExiting.\n"); exit(1); } }
	free(short_temp); short_temp = NULL;
	
	return;
}

/****************************************************************** Comapare Header Info *****************************************************************/	
	
void		compare_header_info(HEADER_INFO *h1, HEADER_INFO *h2)
{
	si4	i, exit_flag;
	
	// exit if difference could compromise data processing
	exit_flag = 0;
		
	if (h1->file_schema != h2->file_schema) fprintf(stderr, "\nWarning: file schema changed\n");
		
	if (strcmp(h1->pt_surname, h2->pt_surname)) 
		fprintf(stderr, "\nWarning: patient surname changed from %s to %s\n", h1->pt_surname, h2->pt_surname);
		
	if (strcmp(h1->pt_firstname, h2->pt_firstname)) fprintf(stderr, "\nWarning: patient first name changed\n");
		
	if (strcmp(h1->pt_middlename, h2->pt_middlename)) 
		fprintf(stderr, "\nWarning: patient middle name changed from %s to %s\n", h1->pt_middlename, h2->pt_middlename);
		
	if (strcmp(h1->pt_id, h2->pt_id)) fprintf(stderr, "\nWarning: patient ID changed\n");
		
	if (h1->samp_freq != h2->samp_freq) { fprintf(stderr, "\nError: sampling frequency changed (%lf, %lf)\n", h1->samp_freq, h2->samp_freq); exit_flag = 1; }
		
	if (h1->num_system_chans != h2->num_system_chans) { fprintf(stderr, "\nError: number of system channels changed\n"); exit_flag = 1; }
	
	if (h1->num_recorded_chans != h2->num_recorded_chans) { fprintf(stderr, "\nError: number of recorded channels changed  h1 %d h2 %d\n", h1->num_recorded_chans, h2->num_recorded_chans); exit_flag = 1; }
		
	if (h1->num_delta_bits != h2->num_delta_bits) { fprintf(stderr, "\nError: number of delta bits changed\n"); exit_flag = 1; }
		
	if (h1->num_headboxes != h2->num_headboxes) { fprintf(stderr, "\nError: number of headboxes changed\n"); exit_flag = 1; }
		
	for (i = 0; i < h1->num_headboxes; ++i) if (h1->headbox_types[i] != h2->headbox_types[i]) fprintf(stderr, "\nWarning: headbox type changed\n");
		
	if (h1->num_discard_bits != h2->num_discard_bits) { fprintf(stderr, "\nError: number of discard bits changed\n"); exit_flag = 1; }
		
	if (h1->microvolts_per_AD_unit != h2->microvolts_per_AD_unit) { fprintf(stderr, "\nError: A/D conversion factor changed\n"); exit_flag = 1; }
		
	for (i = 0; i < h1->num_system_chans; ++i) 
		if (h1->rec_to_phys_chan_map[i] != h2->rec_to_phys_chan_map[i]) { 
			fprintf(stderr, "\nError: recorded to physical channel map changed\n"); 
			exit_flag = 1; 
		}
	
	if (exit_flag && override_flag==0) //if override_flag is set, keep plugging along until all hell breaks loose
		exit(1);
	
	return;
}

/******************************************* Write out video sync and annotation events for current directory ********************************************/

void	write_event_data(GLOBALS *globals, si4 dir_idx)
{
	si1		*raw_event_data, *vtc_event_data, temp_str[1024], vtc_guid[16];
	si1		vtc_guid_avi[GUID_BYTES] = VTC_GUID_AVI;
	si1		vtc_guid_mpg[GUID_BYTES] = VTC_GUID_MPG;
	si4		i, j, fid, read_ent_events(), read_vtc_events();
	si4		vid_events, event_cnt, max_events, vid_file_exists, epo_file_exists;
	ui4		rec_len, rev_ui4();
	ui8		note_flen, vid_flen, nr;
	FILE		*fp;
	EVENT_PTR	*event_ptrs;
	int		compare_event_times();
#ifndef _WIN32
	struct stat	sb;
#else
    struct _stat64	sb;
#endif
					
	// read the ".ent" file
	sprintf(temp_str, "%s/%s/%s.ent", globals->dir_list[dir_idx].path, globals->dir_list[dir_idx].name, globals->dir_list[dir_idx].name);
	fp = fopen(temp_str, "rb");
	if (fp == NULL) { (void) fprintf(stderr, "\n\nCould not open the file %s\nExiting.\n", temp_str); exit(1); }
	
#ifndef _WIN32
    fid = fileno(fp);
    fstat(fid, &sb);
#else
    fid = _fileno(fp);
    _fstat64(fid, &sb);
#endif
	note_flen = sb.st_size - GENERIC_HEADER_END_OFFSET;
	fseek(fp, GENERIC_HEADER_END_OFFSET, SEEK_SET);
	
	raw_event_data = (si1 *) malloc(note_flen);
	if (raw_event_data == NULL) { (void) fprintf(stderr, "\n\nInsufficient memory\nExiting.\n"); exit(1); }

	nr = fread(raw_event_data, sizeof(si1), (size_t) note_flen, fp);
	if (nr != note_flen) { 
        (void) fprintf(stderr, "\n\nError reading the file %s\n\Exiting.n", temp_str); 
        exit(1); 
    }
	fclose(fp);
	
	// read the ".vtc" file	
	vid_file_exists = 1;
	vtc_event_data = NULL;
	sprintf(temp_str, "%s/%s/%s.vtc", globals->dir_list[dir_idx].path, globals->dir_list[dir_idx].name, globals->dir_list[dir_idx].name);
	fp = fopen(temp_str, "rb");
	if (fp == NULL) { 
		(void) fprintf(stderr, "Warning: Could not open the file %s\n\n", temp_str); 
		vid_file_exists = 0;
		vid_flen = 0;
		vid_events = 0;

	} else {
		//Read VTC GUID to determine VTC file type - first 16 bytes of file
		//length of the VTC entries depends upon file type. 
		fseek(fp, GUID_OFFSET, SEEK_SET);
		nr = fread(vtc_guid, sizeof(si1), GUID_BYTES, fp);
		if ( memcmp(vtc_guid, vtc_guid_avi, GUID_BYTES) == 0 ){
			globals->VTC_Record_Length = XLTEK_VTC_RECORD_LENGTH_AVI;
		}
		else if(memcmp(vtc_guid, vtc_guid_mpg, GUID_BYTES) == 0) {
			globals->VTC_Record_Length = XLTEK_VTC_RECORD_LENGTH_MPG;
		}
		else {
			fprintf(stderr, "Error: Unrecognized VTC GUID in file %s\n\n", temp_str);
			vid_file_exists = 0;
			vid_flen = 0;
		}

#ifndef _WIN32
        fid = fileno(fp);
        fstat(fid, &sb);
#else
        fid = _fileno(fp);
        _fstat64(fid, &sb);
#endif
		vid_flen = sb.st_size - VTC_HEADER_LENGTH;
		fseek(fp, VTC_HEADER_LENGTH, SEEK_SET);
		
		vid_events = vid_flen / globals->VTC_Record_Length;
		vtc_event_data = (si1 *) malloc(vid_flen);
		if (vtc_event_data == NULL) { (void) fprintf(stderr, "\n\nInsufficient memory\nExiting.\n"); exit(1); }

		nr = fread(vtc_event_data, sizeof(si1), (size_t) vid_flen, fp);
		if (nr != vid_flen) { 
            (void) fprintf(stderr, "\n\nError reading the file %s\nExiting.\n", temp_str);
            exit(1); 
        }
		fclose(fp);
	}

    epo_file_exists = 1;
    sprintf(temp_str, "%s/%s/%s.epo", globals->dir_list[dir_idx].path, globals->dir_list[dir_idx].name, globals->dir_list[dir_idx].name);
    // check if .epo file exists
    fp = fopen(temp_str, "rb");
    if (fp == NULL)
        epo_file_exists = 0;
    else
        fclose(fp);
	
	//printf("\nset event ptr array\n");
	// set up event pointer array
	max_events = (note_flen / 250) + vid_events + MAX_SLEEP_STAGE_EVENTS;     // Note: event number is an estimate based on a little sampling - could be wrong
	event_ptrs = (EVENT_PTR *) calloc((size_t) max_events, sizeof(EVENT_PTR));
	if (event_ptrs == NULL) { (void) fprintf(stderr, "\n\nInsufficient memory\nExiting.\n"); exit(1); }
	
	// read the ".ent" events
	event_cnt = read_ent_events(event_ptrs, globals, raw_event_data, note_flen, 0);
    //fprintf(stderr, "number of events: %d\n", event_cnt);

	// read the ".vtc" events
	if (vid_file_exists)
		//event_cnt = read_vtc_events(event_ptrs, globals, vtc_event_data, vid_events, event_cnt);
        event_cnt = read_vtc_events(event_ptrs, globals, vtc_event_data, vid_events, event_cnt, dir_idx);

    if (epo_file_exists)
        event_cnt = read_epo_file(event_ptrs, globals, event_cnt, dir_idx);
	
	// sort events and write out
	qsort((void *) event_ptrs, (size_t) event_cnt, sizeof(EVENT_PTR), compare_event_times);
	fp = globals->efp;

	for (i = 0; i < event_cnt; ++i) {
		rec_len = rev_ui4((ui4)event_ptrs[i].rec_len); //event_ptrs[i].rec_len is big-endian
		//nr = fwrite(event_ptrs[i].event, sizeof(si1), (size_t)rec_len, fp);
        
        if (event_ptrs[i].duration == 0)
#ifdef _WIN32
            fprintf(fp, "%lld,%s\n", event_ptrs[i].mayo_time, event_ptrs[i].note_text);
#else
            fprintf(fp, "%ld,%s\n", event_ptrs[i].mayo_time, event_ptrs[i].note_text);
#endif
        else
#ifdef _WIN32
            fprintf(fp, "%lld,%s (sleep stage)\n", event_ptrs[i].mayo_time, event_ptrs[i].note_text);
#else
            fprintf(fp, "%ld,%s (sleep stage)\n", event_ptrs[i].mayo_time, event_ptrs[i].note_text);
#endif

        //fprintf(stderr, "%d ...%ld,%s...\n", i, event_ptrs[i].mayo_time, event_ptrs[i].note_text);
		//if (nr != rec_len) { (void) fprintf(stderr, "\n\nError writing event file\nExiting.\n"); exit(1); }
#ifdef OUTPUT_TO_MEF3
        // .mefd gets added by create_or_append_annotations()
        if (globals->output_directory == NULL)
            sprintf(temp_str, "mef3");
        else
            sprintf(temp_str, "%s", globals->output_directory);
        //fprintf(stderr, "writing record...");

        // open annotation files before the first event is written
        if (i == 0)
            create_or_append_annotations(&(globals->annotation_state), temp_str, -6.0, "not_entered");

        // write annotations event
        if (event_ptrs[i].duration == 0)
            write_annotation(&(globals->annotation_state), event_ptrs[i].mayo_time, "Note", event_ptrs[i].note_text);
        else
        {
            // do epoch (sleep stage, in this case)
            MEFREC_Epoc_1_0* mefrec_epoc = calloc(1, sizeof(MEFREC_Epoc_1_0));
            mefrec_epoc->id_number = event_ptrs[i].id_number;
            mefrec_epoc->duration = event_ptrs[i].duration;
            mefrec_epoc->timestamp = event_ptrs[i].mayo_time;
            mefrec_epoc->end_timestamp = event_ptrs[i].mayo_time + event_ptrs[i].duration;
            strcpy(mefrec_epoc->text, event_ptrs[i].note_text);
            strcpy(mefrec_epoc->epoch_type, "sleep stage");

            write_annotation(&(globals->annotation_state), event_ptrs[i].mayo_time, "Epoc", mefrec_epoc);

            free(mefrec_epoc);
        }

        // close annotation files after the last event is written
        if (i == (event_cnt - 1))
            close_annotation(&(globals->annotation_state));
#endif
	}

	
	globals->tot_num_events += event_cnt;
	free(raw_event_data); raw_event_data=NULL;
	free(vtc_event_data); vtc_event_data=NULL;
	free(event_ptrs); event_ptrs=NULL;
	
	return;
}


#ifndef _WIN32
int isDirectory(const char* path)
{
    struct stat statbuf;
    if (stat(path, &statbuf) != 0)
        return 0;
    return S_ISDIR(statbuf.st_mode);
}
#endif

si4 read_epo_file(EVENT_PTR* event_ptrs, GLOBALS* globals, si4 event_cnt, si4 dir_idx)
{
    si1 temp_epo_folder[4096];
    si1 temp_epo_folder_sub[4096];
    si1 temp_epo_folder_iterate[4096];
    si1 temp_epo_folder_sub_iterate[4096];
    si1 subfolder_seeking[4096];
    si1 temp_str[1024];

    const char* stage_names[] = { "Wake", "NREM1", "NREM2", "NREM3", "Unknown 4", "REM", "Unknown 6", "Unknown 7", "Unscored", "Unknown 9", "Unknown 10" };

    si1 global_file[4096];
    FILE* fp;
    FILE* fp2;
    long buffer_size;
    char* buffer;
    si4 result;
    int value;
    si4 curr_sync_idx;

    si4 epoch_length;
    si4 total_epochs;
    si4 start_time_epoch_1;
    si4 epoch_offset;
    SYNC_INFO* sync_info;
    ui8 temp_time;
    sf8 samp_delta;
    si4 i;
    si8 samp_stamp_counter;
    si1 lpTempPathBuffer[1024];
    si1* str;
    si1 command[1024];


    si4 samp_stamp;
    curr_sync_idx = 0;

    sync_info = globals->sync_info;
    samp_delta = globals->samp_delta;

    // copy epo file to temp directory


    sprintf(temp_str, "%s/%s/%s.epo", globals->dir_list[dir_idx].path, globals->dir_list[dir_idx].name, globals->dir_list[dir_idx].name);
    // check if .epo file exists
    fp = fopen(temp_str, "rb");
    if (fp == NULL)
        return event_cnt;
    else
        fclose(fp);


    // get path with random UUID
#ifdef _WIN32
    GetTempPathA(MAX_PATH, lpTempPathBuffer);
#else
    // TODO: $TMPDIR variable might be better
    sprintf(lpTempPathBuffer, "/tmp/");
#endif
    str = calloc(33, sizeof(si1));
    srand(time(NULL));
    for (i = 0; i < 32; i++)
    {
        str[i] = (rand() % 16) + 97;
        if (str[i] >= 103) str[i] -= 55;
    }

    sprintf(temp_epo_folder, "%s%s", lpTempPathBuffer, str);

    // Decompress .epo file to temp folder
    // temp_str -> temp_epo_folder



    sprintf(command, "mkdir %s", temp_epo_folder);
    // use only backslashes here, because the "copy" command in Windows 10 is very picky about it.  TBD: check on other Windows versions
#ifdef _WIN32
    slash_to_backslash(&command);
#endif
    system(command);

#ifdef _WIN32
    sprintf(command, "copy \"%s\" %s", temp_str, temp_epo_folder);
#else
    sprintf(command, "cp \"%s\" %s", temp_str, temp_epo_folder);
#endif
#ifdef _WIN32
    slash_to_backslash(&command);
#endif
    system(command);

#ifdef _WIN32
    sprintf(command, "\"\"c:\\program files\\7-zip\\7zG\" x \"%s/%s.epo\" -o%s\"", temp_epo_folder, globals->dir_list[dir_idx].name, temp_epo_folder);
#else
    sprintf(command, "./7zz x \"%s/%s.epo\" -o%s", temp_epo_folder, globals->dir_list[dir_idx].name, temp_epo_folder);
    fprintf(stderr, "Command to run: %s\n", command);
#endif
#ifdef _WIN32
    slash_to_backslash(&command);
#endif
    system(command);




    sprintf(global_file, "%s/GlobalHeader/EpochInfo", temp_epo_folder);

    fp = fopen(global_file, "rb");

    if (fp == NULL)
    {
        fputs("Sleep stage (EPO) error - likely couldn't expand .epo file into temp directory.\n", stderr);
        return event_cnt;
    }

    fseek(fp, 0, SEEK_END);
#ifndef _WIN32
    buffer_size = ftell(fp);
#else
    buffer_size = _ftelli64(fp);
#endif
    rewind(fp);

    buffer = (char*)malloc((sizeof(char)) * buffer_size);
    if (buffer == NULL)
    {
        fputs("EPO Memory error", stderr);
        return event_cnt;
    }

    result = fread(buffer, 1, buffer_size, fp);
    if (result != buffer_size)
    {
        fputs("EPO Reading error", stderr);
        return event_cnt;
    }

    fclose(fp);

    memcpy(&epoch_length, buffer, sizeof(si4));
    memcpy(&total_epochs, buffer + 4, sizeof(si4));
    memcpy(&start_time_epoch_1, buffer + 12, sizeof(si4));
    memcpy(&epoch_offset, buffer + 77, sizeof(si4));

    free(buffer);


    si8 first_epoch_uutc;
    samp_stamp = start_time_epoch_1;


    while (samp_stamp >= sync_info[curr_sync_idx + 1].samp_stamp)
        ++curr_sync_idx;
    temp_time = sync_info[curr_sync_idx].samp_time + (ui8)(((sf8)(samp_stamp - sync_info[curr_sync_idx].samp_stamp) * samp_delta) + (sf8)0.5);



#ifdef _WIN32
    sprintf(temp_epo_folder_iterate, "%s\\*", temp_epo_folder);
    subfolder_seeking[0] = 0;

    WIN32_FIND_DATAA FindFileData;
    HANDLE hFind = FindFirstFileA(temp_epo_folder_iterate, &FindFileData);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        return event_cnt;
    }

    do {
        if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            sprintf(temp_epo_folder_sub, "%s\\%s\\*", temp_epo_folder, FindFileData.cFileName);
            if (!strcmp("..", FindFileData.cFileName))
                continue;
            if (!strcmp(".", FindFileData.cFileName))
                continue;
            WIN32_FIND_DATAA FindFileData2;
            HANDLE hFind2 = FindFirstFileA(temp_epo_folder_sub, &FindFileData2);
            if (hFind2 == INVALID_HANDLE_VALUE)
            {
                continue;
            }
            do {
                if (!strcmp("..", FindFileData2.cFileName))
                    continue;
                if (!strcmp(".", FindFileData2.cFileName))
                    continue;
                if (!strcmp("ManualAutoData", FindFileData2.cFileName))
                    sprintf(subfolder_seeking, FindFileData.cFileName);
                //if (!(FindFileData2.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                   // fprintf(stderr, " %s: %s\n", FindFileData.cFileName, FindFileData2.cFileName);
            } while (FindNextFileA(hFind2, &FindFileData2));

            FindClose(hFind2);

        }

    } while (FindNextFileA(hFind, &FindFileData) != 0);

    FindClose(hFind);
#else
    DIR* folder;
    struct dirent* entry;
    int files = 0;
    DIR* folder2;
    struct dirent* entry2;
    char full_entry_name[4096];

    //sprintf(temp_epo_folder_iterate, "%s\\*", temp_epo_folder);
    subfolder_seeking[0] = 0;

    folder = opendir(temp_epo_folder);
    if (folder == NULL)
    {
        return event_cnt;
    }

    while ((entry = readdir(folder)))
    {
        sprintf(full_entry_name, "%s/%s", temp_epo_folder, entry->d_name);
        if (isDirectory(full_entry_name))
        {
            sprintf(temp_epo_folder_sub, "%s/%s/", temp_epo_folder, entry->d_name);
            if (!strcmp("..", entry->d_name))
                continue;
            if (!strcmp(".", entry->d_name))
                continue;
            
            folder2 = opendir(temp_epo_folder_sub);
            if (folder2 == NULL)
            {
                continue;
            }
            while ((entry2 = readdir(folder2)))
            {
                if (!strcmp("..", entry2->d_name))
                    continue;
                if (!strcmp(".", entry2->d_name))
                    continue;
                if (!strcmp("ManualAutoData", entry2->d_name))
                    sprintf(subfolder_seeking, entry->d_name);
                //if (!(FindFileData2.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                   // fprintf(stderr, " %s: %s\n", FindFileData.cFileName, FindFileData2.cFileName);
            } 

            closedir(folder2);

        }

    }

    closedir(folder);
#endif


    fprintf(stderr, "Subfolder with sleep staging: %s\n", subfolder_seeking);

    sprintf(global_file, "%s/%s/ChannelRawData", temp_epo_folder, subfolder_seeking);

    fp = fopen(global_file, "rb");

    if (fp == NULL)
    {
        return event_cnt;
    }

    fseek(fp, 0, SEEK_END);
#ifndef _WIN32
    buffer_size = ftell(fp);
#else
    buffer_size = _ftelli64(fp);
#endif
    rewind(fp);

    buffer = (char*)malloc((sizeof(char)) * buffer_size);
    if (buffer == NULL)
    {
        fputs("EPO Memory error", stderr);
        return event_cnt;
    }

    result = fread(buffer, 1, buffer_size, fp);
    if (result != buffer_size)
    {
        fputs("EPO Reading error", stderr);
        return event_cnt;
    }

    if (buffer_size < (total_epochs + epoch_offset))
    {
        fputs("EPO Reading error, not enough epochs in ChannelRawData file.", stderr);
        return event_cnt;
    }





    if (globals->output_directory == NULL)
        sprintf(temp_str, "mef3");
    else
        sprintf(temp_str, "%s", globals->output_directory);


    char epo_file_name[1024];
    char new_line[1024];
    sprintf(epo_file_name, "%s.mefd/sleep_stages.csv", temp_str);

    //fp2 = fopen(epo_file_name, "ab+");


    curr_sync_idx = 0;
    samp_stamp_counter = start_time_epoch_1;
    for (i = epoch_offset; i < (total_epochs + epoch_offset); i++)
    {
        si8 start_time, end_time;

        while (samp_stamp_counter >= sync_info[curr_sync_idx + 1].samp_stamp)
            ++curr_sync_idx;
        temp_time = sync_info[curr_sync_idx].samp_time + (ui8)(((sf8)(samp_stamp_counter - sync_info[curr_sync_idx].samp_stamp) * samp_delta) + (sf8)0.5);

        start_time = temp_time;
        end_time = temp_time + (epoch_length * 1000000);
#ifndef _WIN32
        fprintf(stderr, "%d %ld\n", (si4)(*(buffer + i)), temp_time);
#else
        fprintf(stderr, "%d %lld\n", (si4)(*(buffer + i)), temp_time);
#endif

#ifndef _WIN32
        sprintf(new_line, "%ld, %ld, %s\n", start_time, end_time, stage_names[(si4)(*(buffer + i))]);
#else
        sprintf(new_line, "%lld, %lld, %s\n", start_time, end_time, stage_names[(si4)(*(buffer + i))]);
#endif
        //fwrite(new_line, sizeof(char), strlen(new_line), fp2);

        samp_stamp_counter += epoch_length * globals->samp_freq;

        // Don't put "Unscored" sleep stage events into output
        if ((*(buffer + i)) == 8)
            continue;

        si4 len = strlen(stage_names[(si4)(*(buffer + i))]);
        event_ptrs[event_cnt].mayo_time = start_time;
        event_ptrs[event_cnt].note_text = (si1*)calloc((size_t)4096, sizeof(si1));
        strncpy(event_ptrs[event_cnt].note_text, stage_names[(si4)(*(buffer + i))], len);
        event_ptrs[event_cnt].duration = epoch_length * 1000000;
        event_ptrs[event_cnt].id_number = (i - epoch_offset) + 1;  // make first epoch "1" so it matches Sleepworks

        ++event_cnt;
    }

    //fclose(fp2);

    fclose(fp);




    free(buffer);

    return event_cnt;

}


/******************************************* Read contents of ".ent" file and create corresponding Mayo event records ********************************************/

#define NUM_STR	64

si4		read_ent_events(EVENT_PTR *event_ptrs, GLOBALS *globals, si1 *raw_event_data, ui8 note_flen, si4 event_cnt)
{
	si1		*data_end, *nedp, *redp, *rec_start, *rec_end, *lb;
	si1		temp_str[NUM_STR][4096], *c, note_text[8192];
	si4		i, len, note_type, rec_len, p_level, str_num;
	si4		text_str_num, stamp_str_num, comm_str_num, samp_stamp;
	ui8		curr_sync_idx, temp_time, rev_ui8();
	sf8		samp_delta;
	SYNC_INFO	*sync_info;
	ui4		rev_ui4();
    si4     in_double_quotes;
    si4     backslash_counter;
	
	curr_sync_idx = 0;
	samp_delta = globals->samp_delta;
	sync_info = globals->sync_info;
	text_str_num = stamp_str_num = comm_str_num = 0;
	
	data_end = raw_event_data + note_flen;
	nedp = redp = raw_event_data;
	while (redp < data_end) {
		// store record start
		rec_start = redp;
		//printf("rec_start %lu %lu\n", *(ui8*)rec_start, rev_ui8(*(ui8*)rec_start));
		
		// get note type: key-tree = 1, end of notes = 0
		lb = (si1 *) &note_type;
		for (i = 4; i--;) *lb++ = *redp++;

		if (note_type != 1) break;
		
		// get record length
		lb = (si1 *) &rec_len;
		for (i = 4; i--;) *lb++ = *redp++;

		rec_end = rec_start + rec_len;
		
		// read key-tree
		redp += 8;      // start of key-tree
		while (*redp++ != '(');  // nothing in level 1 parentheses
		while (*redp++ != '(');  // find first level 2 parenthesis


		p_level = 2;
		str_num = 0;
        
        // clear temp strings
        for (i=0;i<NUM_STR;i++)
            memset(temp_str[i], '\0', 4096);
                   
		c = temp_str[0];
        in_double_quotes = 0;
        backslash_counter = 0;
		while (redp < data_end) {
            if (*redp == '"')
            {
                if (in_double_quotes)
                {
                    // if the number if immediately preceeding backslashes is odd, then this double quotes is actually a double_quote character
                    if (backslash_counter % 2 == 0)
                        in_double_quotes = 0;
                }
                else
                    in_double_quotes = 1;
            }
			
            if (*redp == '"') { goto done_with_letter; }
            if (!in_double_quotes)
            {
                if (*redp == '.' || *redp == ',') { goto done_with_letter; }
            }
            if (!in_double_quotes)
            {
                if (*redp == '.') { if (*(redp - 1) == '(') { goto done_with_letter; } }
            }
            if (!in_double_quotes) // necessary because sometimes an annotation contains a ( or ) that isn't matched, ie, manual typo
            {
                if (*redp == '(') { ++p_level; goto done_with_letter; }
                if (*redp == ')') { --p_level; if (p_level == 1) { *c = 0; c = temp_str[++str_num]; } goto done_with_letter; }
            }
			if (p_level == 2) *c++ = *redp;
			if (p_level == 0) break;

        done_with_letter:

            if (*redp == '\\')
                backslash_counter++;
            else
                backslash_counter = 0;

			++redp;
		}
		if (p_level) { goto skip_note; } // error in .ent file
		
        
        comm_str_num = -1;
		// find "Text", "Stamp", and "Comment" note fields 
		for (i = NUM_STR - 1; i >= 0; --i) {

			if (!strncmp(temp_str[i], "Text", 4)) {
				if (!strncmp(temp_str[i] + 5, "XLEvent", 7)) goto skip_note;
                if (!strncmp(temp_str[i] + 5, "XLSpike", 7)) goto skip_note;
				if (!strncmp(temp_str[i] + 5, "Started Analyzer", 16)) goto skip_note;
                if (!strncmp(temp_str[i] + 5, "Gain/Filter Change", 18)) goto skip_note;
                if (!strncmp(temp_str[i] + 5, "Montage:", 8)) goto skip_note;
                if (!strncmp(temp_str[i] + 5, "Recording Analyzer", 18)) goto skip_note;
                if (!strncmp(temp_str[i] + 5, "Analyzer info - Persyst", 23)) goto skip_note;
                if (!strncmp(temp_str[i] + 5, "Persyst Spike", 13)) goto skip_note;
                if (!strncmp(temp_str[i] + 5, "Impedance at", 12)) goto skip_note;
                if (!strncmp(temp_str[i] + 5, "Persyst Seizure", 15)) goto skip_note;
                if (!strncmp(temp_str[i] + 5, "Spike", 5)) goto skip_note;
				text_str_num = i;
				continue;
			}
			if (!strncmp(temp_str[i], "Stamp", 5)) { stamp_str_num = i; continue; }
			if (!strncmp(temp_str[i], "Comment", 7)) { comm_str_num = i; continue; }
		}

        // check for case where there is no Comment field
        if (comm_str_num == -1)
            sprintf(note_text, "%s", temp_str[text_str_num] + 5);
        else
        {
            // look to see if Comment contains anything besides empty string, if so, put it in parenthesis.
            // not sure if this first test is necessary, but it is legacy code
            if (*(temp_str[comm_str_num] + 7))
            {
                //fprintf(stderr, "len text %d len comm %d\n", strlen(temp_str[text_str_num]), strlen(temp_str[comm_str_num]));
                
                // limit size of notes to prevent buffer overflows
                if (strlen(temp_str[text_str_num]) > 4000)
                    temp_str[text_str_num][4000] = 0;
                if (strlen(temp_str[comm_str_num]) > 4000)
                    temp_str[text_str_num][4000] = 0;
                
                // this test prevents blank comments from being put in parenthesis
                if (strlen(temp_str[comm_str_num] + 8) > 0)
                    sprintf(note_text, "%s (%s)", temp_str[text_str_num] + 5, temp_str[comm_str_num] + 8);
                else
                    sprintf(note_text, "%s", temp_str[text_str_num] + 5);
            }
            else
                sprintf(note_text, "%s", temp_str[text_str_num] + 5);
        }

		// substitute spaces for tabs
		c = note_text - 1;
		while (*++c) { if (*c == '\t') *c = ' '; }
        
        // substitute spaces for commas
        c = note_text - 1;
        while (*++c) { if (*c == ',') *c = ' '; }

        // remove all newlines
        c = note_text - 1;
        while (*++c) { if (*c == '\n') *c = ' '; }

        // remove "\xd"
        c = note_text - 1;
        while (*++c) { if (*c == '\\') { if (*(c + 1) == 'x' && *(c + 2) == 'd') { c += 3; len = strlen(c) + 1; memmove(c - 3, c, len); } } }
		
        // remove multiple internal white spaces, newlines, and periods
        c = note_text;
        while (*c++) { if ((*c == ' ' && *(c - 1) == ' ') || (*c == '\n' && *(c - 1) == '\n') || (*c == '.' && *(c - 1) == '.')) { len = strlen(c) + 1; memmove(c - 1, c, len); c -= 2; } }

        // obscure mayo clinic 'M' numbers, by x'ing out the first 5 digits after the m/M
        c = note_text - 1;
        while (*++c) {
            if (*c == ' ' && *(c + 1) == 'B' && *(c + 2) == 'y' && *(c + 3) == ':' && *(c+4) == ' ' && (*(c + 5) == 'm' || *(c + 5) == 'M'))
            {
                *(c + 6) = 'x';
                *(c + 7) = 'x';
                *(c + 8) = 'x';
                *(c + 9) = 'x';
                *(c + 10) = 'x';
                if ((*c + 11) != ' ')  // assuming all m numbers are at least 5 digits, but possibly 6 digits as well.
                    *(c + 11) = 'x';
            }
        }

		// remove leading white spaces
        // commented out by DPC - this leads sometimes to abort trap, since not all notes are >= 12 in length, so fix logic before putting back in
		//while (note_text[12] == ' ') {len = strlen(note_text) - 12; memmove(note_text + 12, note_text + 13, len); }


		// capitalize first letter
		//if (note_text[12] >= 96) note_text[12] -= 32;
		
		// create Mayo event record
		event_ptrs[event_cnt].event = nedp;
		*nedp++ = 'M'; *nedp++ = 'N'; *nedp++ = 'T'; *nedp++ = 0;    // Event type code field (MNT)
		len = strlen(note_text) + 1;
		while (len % 8)
			note_text[len++] = 0;     // ensure length is multiple of 8, including terminal zero
		
		// Event record size field- little endian
		event_ptrs[event_cnt].rec_len = *((ui4 *) nedp) = rev_ui4((ui4) (len + 16));
		nedp += 4;
		samp_stamp = atoi(temp_str[stamp_str_num] + 6);
		if (samp_stamp >= sync_info[curr_sync_idx + 1].samp_stamp)
			++curr_sync_idx;
		temp_time = sync_info[curr_sync_idx].samp_time + (ui8) (((sf8) (samp_stamp - sync_info[curr_sync_idx].samp_stamp) * samp_delta) + (sf8) 0.5);
		event_ptrs[event_cnt].mayo_time = temp_time;  // Event time field must stay LITTLE-ENDIAN for comparison
		*((ui8 *) nedp) = rev_ui8(temp_time);  // Event time field in mvf file record must be BIG-ENDIAN
		nedp += 8;
		memmove(nedp, note_text, len);	  // Note text field
        
        // for MEF2, MEF3:
        event_ptrs[event_cnt].note_text = (si1 *) calloc((size_t) 4096, sizeof(si1));
        strncpy(event_ptrs[event_cnt].note_text, note_text, len);
        // replace newlines with spaces
        for (i=0;i<strlen(event_ptrs[event_cnt].note_text);i++)
        {
            if (event_ptrs[event_cnt].note_text[i] == '\n')
                event_ptrs[event_cnt].note_text[i] = ' ';
            if (event_ptrs[event_cnt].note_text[i] == 10)  // line feed
                event_ptrs[event_cnt].note_text[i] = ' ';
        }

        //fprintf(stderr, "%d ...%s...\n", event_cnt, event_ptrs[event_cnt].note_text);
        
		nedp += len;
		++event_cnt;
		
		skip_note:
		if (redp > rec_end)
			fprintf(stderr, "\n\nWarning: Error reading notes file\n\n");
		redp = rec_end;	
	}
	
	return(event_cnt);
}

/******************************************* Read contents of ".vtc" file and create corresponding Mayo event records ********************************************/

char* strrstr(const char* str, const char* strsearch)
{
    char* ptr, * last = NULL;
    ptr = (char*)str;
    while ((ptr = strstr(ptr, strsearch)))
        last = ptr++;
    return last;
}

si4		read_vtc_events(EVENT_PTR *event_ptrs, GLOBALS *globals, si1 *vtc_event_data, si4 vid_events, si4 event_cnt, si4 dir_idx)
{
	si4	i, j, len;
	ui4	rev_ui4();
	si1	*redp, /* *nedp, */ *hb, *lb, file_name[264];
    si1 full_file_name[1024];
	ui8	time_correction_factor, temp_time, rev_ui8();
    si8 start_time;
    si8 end_time;
    si1 temp_str[1024];
    si1 command[1024];
    si1* extension;
    si4 width, height, num_frames;
    si8 file_size;
    sf8 frame_rate;

	//nedp = redp = vtc_event_data;
    redp = vtc_event_data;
	time_correction_factor = (ui8) XLTEK_TIME_CORRECTION_FACTOR * 1000000;
	for (i = 0; i < vid_events; ++i) {

        /*
		strcpy(file_name, redp);
		event_ptrs[event_cnt].event = nedp;
		*nedp++ = 'V'; *nedp++ = 'S'; *nedp++ = '1'; *nedp++ = 0;    // Event type code field (VS1)
		len = strlen(file_name);
		sprintf(&file_name[len - 3], "mp4");;     // change to ".mp4"		
		//file_name[len - 1] = '4';     // change to ".mp4"
		while (len % 8)
			file_name[len++] = 0;     // ensure length is multiple of 8, including terminal zero
		event_ptrs[event_cnt].rec_len = *((ui4 *) nedp) = rev_ui4((ui4) (len + 32));     // Event record size field
		nedp += 4;////////
		lb = redp + globals->VTC_Record_Length - 16; //start and end times are the last 16 bytes of the record
		hb = (si1 *) &temp_time;
		for (j = 8; j--;) *hb++ = *lb++;
		temp_time = (temp_time + 5) / 10;
		temp_time -= time_correction_factor;     // convert to Mayo time;
		event_ptrs[event_cnt].mayo_time = temp_time;  // Event start time field - keep this LITTLE-ENDIAN for use without requiring byte reversal
		temp_time = rev_ui8(temp_time);//mvf file times must be BIG-ENDIAN
		*((ui8 *) nedp) = temp_time;
		nedp += 8;
		lb = redp + globals->VTC_Record_Length - 8;
		hb = (si1 *) &temp_time;
		for (j = 8; j--;) *hb++ = *lb++;
		temp_time = (temp_time + 5) / 10;
		temp_time -= time_correction_factor; // convert to Mayo time;
		temp_time = rev_ui8(temp_time);//End time in .mvf file must be BIG-ENDIAN
		*((ui8 *) nedp) = temp_time; // Event end time field
		nedp += 8;
		*nedp++ = 'M'; *nedp++ = 'P'; *nedp++ = 'E'; *nedp++ = 'G'; *nedp++ = '4'; *nedp++ = 0; *nedp++ = 0;    // Video format field (MPEG4, part 10)
		*nedp++ = 0;    // Camera view code field (frontal)
		memmove(nedp, file_name, len);	  // File name field
		nedp += len;
		
		++event_cnt;
		redp += globals->VTC_Record_Length;
        */


        // get video file information
        strcpy(file_name, redp);
        len = strlen(file_name);

        // calculate uutc start time of video file
        lb = redp + globals->VTC_Record_Length - 16; //start and end times are the last 16 bytes of the record
        hb = (si1*)&temp_time;
        for (j = 8; j--;) *hb++ = *lb++;
        temp_time = (temp_time + 5) / 10;
        temp_time -= time_correction_factor;     // convert to Mayo time;
        start_time = temp_time;  // Event start time field - keep this LITTLE-ENDIAN for use without requiring byte reversal

        // calculate uutc end time of video vile
        lb = redp + globals->VTC_Record_Length - 8;
        hb = (si1*)&temp_time;
        for (j = 8; j--;) *hb++ = *lb++;
        end_time = (temp_time + 5) / 10;
        end_time -= time_correction_factor; // convert to Mayo time;

        // advance to next video entry
        redp += globals->VTC_Record_Length;

        // convert to MEF 3:

#ifdef OUTPUT_TO_MEF3
        if (globals->output_directory == NULL)
            sprintf(temp_str, "mef3");
        else
            sprintf(temp_str, "%s", globals->output_directory);

        sprintf(full_file_name, "%s/%s/%s", globals->dir_list[dir_idx].path, globals->dir_list[dir_idx].name, file_name);

        FILE *fp;
        char video_csv_name[1024];
        char new_line[1024];
        sprintf(video_csv_name, "%s.mefd/video_timestamps.csv", temp_str);

        fp = fopen(video_csv_name, "ab+");
        if (globals->video_segment_counter != 0)
        {
            sprintf(new_line, "\n", start_time, file_name);
            fwrite(new_line, sizeof(char), strlen(new_line), fp);
        }
#ifndef _WIN32
        sprintf(new_line, "%ld,%s", start_time, file_name);
#else
        sprintf(new_line, "%lld,%s", start_time, file_name);
#endif
        fwrite(new_line, sizeof(char), strlen(new_line), fp);
        fclose(fp);
		
        if (globals->convert_video == 0)
        {
            globals->video_segment_counter++;
            continue;
        }
#ifdef _WIN32
        if (_access(full_file_name, 0) != -1)  // check for existence of video
#else
        if (access(full_file_name, 0) != -1)  // check for existence of video
#endif
        {
            FILE* pPipe;
            si1 psBuffer[128];
            si1* this_char_ptr;
            si1* next_char_ptr;
            si4 file_size;
            si4 numerator;
            si4 denominator;
            si1* mid_char_ptr;

            /*
            // get info from file using ffprobe
            sprintf(command, "C:\\Users\\M094555\\Documents\\ffprobe -v error -select_streams v:0 -show_entries stream=width,height,r_frame_rate,nb_frames -of csv=s=x:p=0 \"%s\"", full_file_name);
            if ((pPipe = _popen(command, "rt")) == NULL)
                continue;
            while (fgets(psBuffer, 128, pPipe))
                puts(psBuffer);
            feof(pPipe);
            printf("%s\n", psBuffer);
            _pclose(pPipe);

            // ffprobe returns info in the form of "1280x720x30/1x3582"
            // TBD add error checking to this
            // TBD verify the ffprobe always returns data in this order?

            // get width
            this_char_ptr = psBuffer;
            next_char_ptr = strstr(this_char_ptr, "x");
            *next_char_ptr = 0;
            width = atoi(this_char_ptr);

            // get height
            this_char_ptr = next_char_ptr + 1;
            next_char_ptr = strstr(this_char_ptr, "x");
            *next_char_ptr = 0;
            height = atoi(this_char_ptr);

            // get frame rate
            this_char_ptr = next_char_ptr + 1;
            next_char_ptr = strstr(this_char_ptr, "x");
            *next_char_ptr = 0;
            mid_char_ptr = strstr(this_char_ptr, "/");
            *mid_char_ptr = 0;
            mid_char_ptr++;
            numerator = atoi(this_char_ptr);
            denominator = atoi(mid_char_ptr);
            frame_rate = ((sf8)numerator) / ((sf8)denominator);

            // get num_frames in video
            this_char_ptr = next_char_ptr + 1;
            num_frames = atoi(this_char_ptr);
            */

            width = -1;
            height = -1;
            num_frames = -1;
            frame_rate = -1;

            extension = strrstr(full_file_name, ".");

            fprintf(stderr, "Copying video segment %d to MEF Video channel.\n", globals->video_segment_counter);
            write_video_file_with_one_clip(temp_str, globals->video_segment_counter++, "Video", full_file_name, start_time, end_time, 
                width, height, num_frames, frame_rate, globals->proto_metadata_fps);
        }
		
#endif


	}
	
	return(event_cnt);
}


/*************************************************** Sorting function for EVENT_PTR structures ******************************************************/

int	compare_event_times(EVENT_PTR *s1, EVENT_PTR *s2)
{
	
	if (s1->mayo_time > s2->mayo_time)
		return(1);
	if (s1->mayo_time < s2->mayo_time)
		return(-1);
	return(0);
}



/*************************************************** Byte Order reversal functions ******************************************************/

#ifdef OUTPUT_TO_MEF3

 si2	rev_si2(si2 x)
{
	ui1	*b1, *b2;
	si2	y;
	
	b1 = ((ui1 *) &x) + 1;
	b2 = (ui1 *) &y;
	*b2++ = *b1--; *b2 = *b1;
	
	return(y);
}

ui4	rev_ui4(ui4 x)
{
	ui1	*pf, *pb;
	ui4	xr;
	
	pf = (ui1 *) &x;
	pb = (ui1 *) &xr + 3;
	
	*pb-- = *pf++;
	*pb-- = *pf++;
	*pb-- = *pf++;
	*pb = *pf;
	
	return(xr);
}

si4	rev_si4(si4 x)
{
	ui1	*b1, *b2;
	si4	y;
	
	b1 = ((ui1 *) &x) + 3;
	b2 = (ui1 *) &y;
	*b2++ = *b1--; *b2++ = *b1--; *b2++ = *b1--; *b2 = *b1;
	
	return(y);
}

sf8	rev_sf8(sf8 x)
{
	ui1	*b1, *b2;
	sf8	y;
	
	b1 = ((ui1 *) &x) + 7;
	b2 = (ui1 *) &y;
	*b2++ = *b1--; *b2++ = *b1--; *b2++ = *b1--; *b2++ = *b1--; *b2++ = *b1--; *b2++ = *b1--; *b2++ = *b1--; *b2 = *b1;
	
	return(y);
}

ui8	rev_ui8(ui8 x)
{
	ui1	*b1, *b2;
	ui8	y;
	
	b1 = ((ui1 *) &x) +7;
	b2 = (ui1 *) &y;
	*b2++ = *b1--; 
	*b2++ = *b1--; 
	*b2++ = *b1--; 
	*b2++ = *b1--; 
	*b2++ = *b1--; 
	*b2++ = *b1--; 
	*b2++ = *b1--; 
	*b2 = *b1;
	
	return(y);
}

void strncpy2(si1 *s1, si1 *s2, si4 n)
{
    si4      len;
    
    for (len = 1; len < n; ++len) {
        if ((*s1++ = *s2++))
            continue;
        return;
    }
    s1[n-1] = 0;
    
    return;
}

#endif

#ifdef OUTPUT_TO_MEF2
void pack_mef_header(MEF_HEADER_INFO *out_header_struct, sf8 secs_per_block, si1 *session_password, si1 *subject_password,
                     si1 *uid, si4 anonymize_flag, si4 dst_flag, si4 bit_shift_flag, sf8 sampling_frequency,
                     si1 *default_first_name, si1 *default_third_name, si1 *default_id, sf8 voltage_conversion_factor)
{
   	si8 l;
    //sf8 voltage_conversion_factor;
    char anonymized_subject_name[ANONYMIZED_SUBJECT_NAME_LENGTH];
    
    // useful constants
    //voltage_conversion_factor = 1.0 / (sf8) (1 << (6 - DISCARD_BITS));
    
    // Set header fields
    strcpy(out_header_struct->institution, "Mayo Clinic, Rochester, MN, USA");
    strcpy(out_header_struct->unencrypted_text_field, "not entered");
    sprintf(out_header_struct->encryption_algorithm,  "AES %d-bit", ENCRYPTION_BLOCK_BITS);
    
    // this is kludgy: there are two ways to specify no password, either NULL pointer, or empty string ("").
    if (subject_password == NULL)
        out_header_struct->subject_encryption_used = 0;
    else
    {
        if (*subject_password)
            out_header_struct->subject_encryption_used = 1;
        else
            out_header_struct->subject_encryption_used = 0;
    }
    
    if (session_password == NULL)
        out_header_struct->session_encryption_used = 0;
    else
    {
        if (*session_password)
            out_header_struct->session_encryption_used = 1;
        else
            out_header_struct->session_encryption_used = 0;
    }
    
    out_header_struct->data_encryption_used = 0; //don't use data encryption by default
    out_header_struct->byte_order_code = 1;      // little-endian
    out_header_struct->header_version_major = (ui1)HEADER_MAJOR_VERSION;
    out_header_struct->header_version_minor = (ui1)HEADER_MINOR_VERSION;
    
    if (uid == NULL)
        memset(out_header_struct->session_unique_ID, 0, SESSION_UNIQUE_ID_LENGTH); //set uid to zeros
    else
        memcpy(out_header_struct->session_unique_ID, uid, SESSION_UNIQUE_ID_LENGTH);
    
    out_header_struct->header_length =  (ui2) MEF_HEADER_LENGTH;
    
    
    /*
    if (anonymize_flag) {
        sprintf(out_header_struct->subject_first_name, "Firstname");
        sprintf(out_header_struct->subject_second_name, "Middlename");
        sprintf(out_header_struct->subject_third_name, "Lastname");
        sprintf(out_header_struct->subject_id, "00-000-000");
    }
    else
    {
        fprintf(stderr, "Subject first name [%s]: ", default_first_name);
        fgets(out_header_struct->subject_first_name, SUBJECT_FIRST_NAME_LENGTH, stdin);
        l = strlen(out_header_struct->subject_first_name) - 1;
        out_header_struct->subject_first_name[l] = 0;
        if (strlen(out_header_struct->subject_first_name) == 0)
        {
            strcpy(out_header_struct->subject_first_name, default_first_name);
        }
        //fprintf(stderr, "%s-\n", out_header_struct->subject_first_name);
        
        fprintf(stderr, "Subject second name []: ");
        fgets(out_header_struct->subject_second_name, SUBJECT_SECOND_NAME_LENGTH, stdin);
        l = strlen(out_header_struct->subject_second_name) - 1;
        out_header_struct->subject_second_name[l] = 0;
        
        fprintf(stderr, "Subject third name [%s]: ", default_third_name);
        fgets(out_header_struct->subject_third_name, SUBJECT_THIRD_NAME_LENGTH, stdin);
        l = strlen(out_header_struct->subject_third_name) - 1;
        out_header_struct->subject_third_name[l] = 0;
        if (strlen(out_header_struct->subject_third_name) == 0)
        {
            strcpy(out_header_struct->subject_third_name, default_third_name);
        }
        //fprintf(stderr, "%s-\n", out_header_struct->subject_third_name);
        
        fprintf(stderr, "Subject ID [%s]: ", default_id);
        fgets(out_header_struct->subject_id, SUBJECT_ID_LENGTH, stdin);
        l = strlen(out_header_struct->subject_id) - 1;
        out_header_struct->subject_id[l] = 0;
        if (strlen(out_header_struct->subject_id) == 0)
        {
            strcpy(out_header_struct->subject_id, default_id);
        }
        //fprintf(stderr, "%s-\n", out_header_struct->subject_id);
    }
     */
    
    strcpy(out_header_struct->subject_first_name, default_first_name);
    strcpy(out_header_struct->subject_third_name, default_third_name);
    strcpy(out_header_struct->subject_id, default_id);
    
    if (session_password)
        strncpy2(out_header_struct->session_password, session_password, SESSION_PASSWORD_LENGTH);
    else
        out_header_struct->session_password[0] = 0;
    
    // Subject_validation_field      is filled in later by build_mef_header_block().
    // Session_validation_field      is filled in later by build_mef_header_block().
    // Protected_Region              is filled in later after build_mef_header_block().
    out_header_struct->number_of_samples = 0;
    // Channel_name                  is filled in later
    // Recording_start_time          is filled in later
    out_header_struct->recording_end_time            = 0;
    out_header_struct->low_frequency_filter_setting  = 0; // TBD: are these filter settings correct?
    out_header_struct->high_frequency_filter_setting = DMA_HIGH_FREQUENCY_FILTER;
    out_header_struct->notch_filter_frequency        = 0;
    
    out_header_struct->sampling_frequency = sampling_frequency;
    out_header_struct->voltage_conversion_factor = voltage_conversion_factor;
    if (bit_shift_flag)
        out_header_struct->voltage_conversion_factor *= 4;
    strcpy(out_header_struct->acquisition_system, "XLTek EEG 128");
    // Channel_comments field        is filled in later
    
    /*fprintf(stderr, "Study comments: ");
    fgets(out_header_struct->study_comments, STUDY_COMMENTS_LENGTH, stdin);
    l = strlen(out_header_struct->study_comments) - 1;
    out_header_struct->study_comments[l] = 0;
    if (out_header_struct->study_comments[0] == 0)
        strcpy((char*)&(out_header_struct->study_comments), "not entered");
     */
    
    
    out_header_struct->physical_channel_number = 0;
    strcpy(out_header_struct->compression_algorithm, "Range Encoded Differences (RED)");
    out_header_struct->maximum_compressed_block_size = 0;
    out_header_struct->maximum_block_length = 0;
    out_header_struct->block_interval = (ui8) ((secs_per_block * 1000000.0) + 0.5);
    out_header_struct->minimum_data_value = 0;
    out_header_struct->maximum_data_value = 0;
    out_header_struct->index_data_offset = 0;
    out_header_struct->number_of_index_entries = 0;
    out_header_struct->block_header_length = BLOCK_HEADER_BYTES;
    if (dst_flag)
        out_header_struct->GMT_offset = -5.0;
    else
        out_header_struct->GMT_offset = -6.0;
    // Discontinuity index           is filled in later
    // Number of discontinuities     is filled in later
    // File Uid                      is filled in later
    memset(anonymized_subject_name, 0, ANONYMIZED_SUBJECT_NAME_LENGTH);  // leave blank for now
    strcpy(out_header_struct->anonymized_subject_name, anonymized_subject_name);
    // Header CRC                    is filled in later
}
#endif



// ***********************
// ** Montage functions **
// ***********************


int find_substring_location_in_buffer(char *buffer, int buffer_size, char* substring, int instance, int start_location)
{
    int found_number;
    int i;
    
    found_number = 0;
    
    for (i=start_location;i<buffer_size - strlen(substring) - 1; i++)
    {
        if (!strncmp(substring, buffer+i, strlen(substring)))
        {
            found_number++;
            
            if (found_number == instance)
                return i;
        }
    }
    
    // substring not found
    return -1;
}

int count_substring_instances_in_buffer(char *buffer, int buffer_size, char* substring)
{
    int i;
    int counter;
    
    counter = 0;
    for (i=0;i<buffer_size - strlen(substring) - 1; i++)
    {
        if (!strncmp(substring, buffer+i, strlen(substring)))
        {
            counter++;
        }
    }
    
    return counter;
}

char* get_between_double_quotes(char *input_string)
{
    char* start;
    char* stop;
    int length;
    char* return_string;
    int i;
    
    start = strstr(input_string, "\"");
    stop  = strstr(input_string + (start - input_string) + 1, "\"");
    
    length = stop - start;
    
    if (length <= 1)
        return NULL;
    
    return_string = (char*) malloc (sizeof(char) * length);
    
    for (i=0;i<length-1;i++)
    {
        return_string[i] = input_string[i + ((start - input_string) + 1)];
        
        // guarantee no spaces in channel names, so replace them with dashes.
        if (return_string[i] == ' ')
            return_string[i] = '-';
        if (return_string[i] == '/')
            return_string[i] = '-';
    }
    
    return_string[length-1] = 0;

    return return_string;
}

char** parse_into_output_montage(char *input_string)
{

    char ** output_montage;
    char* chan_name;
    int counter;
    
    counter = 0;
    
    output_montage = (char**) malloc (sizeof(char*) * MONTAGE_MAX_CHANS);
    
    for (int i=0;i<MONTAGE_MAX_CHANS;i++)
        output_montage[i] = NULL;
    
    chan_name = strtok(input_string, ",");
    
    while (chan_name != NULL)
    {
        output_montage[counter] = get_between_double_quotes(chan_name);
        counter++;

        //fprintf(stderr, "token: %s\n", fixed_chan_name);
        chan_name = strtok(NULL, ",");
    }
    
    return output_montage;
}

int compare_montages(char **input_a, char **input_b)
{
    int i;
    
    for (i=0;i<MONTAGE_MAX_CHANS;i++)
    {
        //fprintf(stderr, "got here 1\n");
        if (input_a[i] == NULL && input_b[i] != NULL)
            return 0;
        if (input_a[i] != NULL && input_b[i] == NULL)
            return 0;
        
        if (input_a[i] != NULL && input_b[i] != NULL)
        {
            if (strcmp(input_a[i], input_b[i]))
                return 0;
        }
    }
    //fprintf(stderr, "got here 2\n");
    return 1;
}

char** find_montage_in_ent(GLOBALS *globals, si4 dir_idx, int instance)
{
    FILE * in_file;
    long buffer_size;
    char * buffer;
    size_t result;
    int i;
    char** output_montage;
    char temp_str[1024];
    
    sprintf(temp_str, "%s/%s/%s.ent", globals->dir_list[dir_idx].path, globals->dir_list[dir_idx].name, globals->dir_list[dir_idx].name);
    in_file = fopen (temp_str, "rb");
    //in_file = fopen ("xxxxxxxxxxx_0b854b0a-2d03-4177-962e-8d646b3cb07b.ent", "rb");

    if (in_file == NULL)
    {
        fputs ("File error", stderr);
        exit (1);
    }
    
    fseek(in_file, 0, SEEK_END);
#ifndef _WIN32
    buffer_size = ftell(in_file);
#else
    buffer_size = _ftelli64(in_file);
#endif
    rewind(in_file);
    
    //printf("\n\nFile is %d bytes big\n\n", buffer_size);
    
    buffer = (char*) malloc ((sizeof(char)) * buffer_size);
    if (buffer == NULL)
    {
        fputs("Memory error", stderr);
        exit(2);
    }
    
    result = fread (buffer, 1, buffer_size, in_file);
    if (result != buffer_size)
    {
        fputs("Reading error",stderr);
        exit(3);
    }
    
    //fprintf(stderr, "done reading ent file\n");

    int location_start = find_substring_location_in_buffer(buffer, buffer_size, "ChanNames", instance, 0);
    
    // if instance isn't found, then we are done
    if (location_start == -1)
        return NULL;
    
    // found instance, so look for end
    int location_end = find_substring_location_in_buffer(buffer, buffer_size, "))", 1, location_start);
    
    location_start += 12;  // skip first 12 characters, which don't contain channel name info
    
    int substring_length = location_end - location_start;
    char *montage_substring = (char*) malloc ((sizeof(char) * substring_length) + 1);
    
    for (i=0;i<substring_length;i++)
        montage_substring[i] = buffer[location_start + i];
    
    montage_substring[substring_length] = 0; // add terminating 0 so tokenizing in subfunctions knows when to stop
    
    // print test output
    //for (i=0;i<substring_length;i++)
    //    fprintf(stderr, "%c", montage_substring[i]);
    
    output_montage = parse_into_output_montage(montage_substring);
    
    
    // print out file, to make sure it is read correctly
    //for (i=0;i<buffer_size;i++)
    //    fprintf(stderr,"%c", buffer[i]);
    
    //if (location >= 0)
    //    fprintf(stderr, "found at %d\n", location);
    //else
    //    fprintf(stderr, "not found\n");
    
    fclose(in_file);
    free(buffer);
    
    return output_montage;
}

int count_montage_in_ent(GLOBALS *globals, si4 dir_idx, si4 output_flag)
{
    FILE * in_file;
    long buffer_size;
    char * buffer;
    size_t result;
    int i;
    char** output_montage;
    char temp_str[1024];
    int return_value;
    
    sprintf(temp_str, "%s/%s/%s.ent", globals->dir_list[dir_idx].path, globals->dir_list[dir_idx].name, globals->dir_list[dir_idx].name);
    in_file = fopen (temp_str, "rb");
    //in_file = fopen ("xxxxxxxxxxx_0b854b0a-2d03-4177-962e-8d646b3cb07b.ent", "rb");
    
    if (in_file == NULL)
    {
        fputs ("File error", stderr);
        exit (1);
    }
    
    fseek(in_file, 0, SEEK_END);
#ifndef _WIN32
    buffer_size = ftell(in_file);
#else
    buffer_size = _ftelli64(in_file);
#endif
    rewind(in_file);
    
    //printf("\n\nFile is %d bytes big\n\n", buffer_size);
    
    buffer = (char*) malloc ((sizeof(char)) * buffer_size);
    if (buffer == NULL)
    {
        fputs("Memory error", stderr);
        exit(2);
    }
    
    result = fread (buffer, 1, buffer_size, in_file);
    if (result != buffer_size)
    {
        fputs("Reading error",stderr);
        exit(3);
    }
    
    return_value = count_substring_instances_in_buffer(buffer, buffer_size, "ChanNames");
    
    if (output_flag)
        fprintf(stderr, "\n%d montages found in file:\n   %s.\n\n", return_value, temp_str);
    
    fclose(in_file);
    free(buffer);
    
    return return_value;

}



// returns 1 if name is in the form C40 C100, C200
// ie, first character must be C and the number must be greater than or equal to 40.
// otherwise, return 0
int skip_montage_name(char *name)
{
    int i;
    
    if (name == NULL)
        return 0;
    
    if (name[0] != 'C')
        return 0;
    
    for (i=1;i<strlen(name);i++)
        if (!isdigit(name[i]))
            return 0;
    
    i = atoi(name+1);
    if (i >= 40)
        return 1;
    
    return 0;
}


void print_usage_message(char *arg)
{
#ifdef OUTPUT_TO_MEF2
    fprintf(stderr, "\nUsage: %s raw_data_directories [-m montage_file] [-o output_directory] [-noprompt] [-video] [-a] [-f]\n\nNote: default output directory is \"mef21/\".\n     -a     Anonymizes the recording: the files will not be encrypted and the patient's name/ID will not be in the output files.\n     -f     Force all recorded channels to be converted (default is to skip generic names such as C100).\n\n\n", arg);
#endif
#ifdef OUTPUT_TO_MEF3
    fprintf(stderr, "\nUsage: %s raw_data_directories [-m montage_file] [-o output_directory] [-noprompt] [-video] [-a] [-f]\n\nNote: default output directory is \"mef3.mefd/\".\nIf specifying an output_directory, \".mefd\" will be added to what is specified.\n   -a        Anonymizes the recording: the files will not be encrypted and the patient's name/ID will not be in the output files.\n   -f        Force all recorded channels to be converted (default is to skip generic names such as C100).\n   -video    Will create a Video.vidd channel which contains the .avi files in MEF 3 format.\n   -noprompt Default answer will be given to all questions.  Noprompt only works when one input directory is specified.\n\n\n", arg);
#endif
}

