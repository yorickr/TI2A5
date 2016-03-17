struct _USER_CONFIG {
    unsigned char len;          /* Length of this structure. */
    unsigned int count;         /* Number of reboots. */
    unsigned int timezone_set;
    int timezone;
	int volume;
    int alarm1_stream_id;
    int alarm2_stream_id;
};

void set_timezone(int timezone);


void save_volume(int);
int get_volume(void);

void set_timezone_set(int val);

int get_timezone_set();

int get_timezone();

int get_bootcount();

int get_alarm1_stream_id();
int get_alarm2_stream_id();

void set_alarm1_stream_id(int id);
void set_alarm2_stream_id(int id);

void memory_init();
