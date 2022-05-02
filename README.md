# xltek2mef
Application to convert eeg studies from Xltek format to Mef3. 

# Linux Users
In XLTek2Mayo.c, after line 4 (include <time.h>) I added another line: #include <sys/resource.h>

# installation instruction

Compile and create xltek2mef3 binary
```
$ chmod +x mkxltek2mayo
$ ./mkxltek2mayo
```

# Running xltek2mef3

```
 $ chmod +x xltek2mef3
 $ xltek2mef3 /path/to/xltek_study -o /path/to/store/mefd -noprompt
```
