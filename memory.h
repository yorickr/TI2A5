struct _USER_CONFIG {
    unsigned char len;          /* Length of this structure. */
    unsigned int count;         /* Number of reboots. */
    unsigned int timezone_set;
    int timezone;
	int volume;
	int bass;
	int treble;
    int alarm1_stream_id;
    int alarm2_stream_id;
};
void set_first_volume(void);
void set_timezone(int timezone);
void factory_reset(void);
void save_volume(int);
int get_volume(void);
void set_timezone_set(int val);
int get_timezone_set(void);
int get_timezone(void);
int get_bootcount(void);

int get_alarm1_stream_id(void);
int get_alarm2_stream_id(void);

void set_alarm1_stream_id(int id);
void set_alarm2_stream_id(int id);

void memory_init(void);

void save_bass(int);

int get_bass(void);

void save_treble(int);

int get_treble(void);

