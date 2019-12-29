#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <getopt.h>
#include <errno.h>
#include <signal.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <alsa/asoundlib.h>

#define NSEC_PER_SEC 1000000000L

static int do_device_list, do_rawmidi_list;
static char *port_name = "default";
static char *send_file_name;
static char *receive_file_name;
static char *send_hex;
static char *send_data;
static int send_data_length;
static int receive_file;
static int dump;
static float timeout;
static int stop;
static int sysex_interval;
static snd_rawmidi_t *input, **inputp;
static snd_rawmidi_t *output, **outputp;

static void list_card_devices(int card, char names[32][32])
{
}

int device_list(char names[32][32])
{
	int card, err;
	snd_ctl_t *ctl;
	char cname[32];
	int device;

	snd_rawmidi_info_t *info;
	const char *name;
	const char *sub_name;
	int subs, subs_in, subs_out;
	int sub;
	int num = 0;

	card = -1;
	if ((err = snd_card_next(&card)) < 0)
	{
		return;
	}
	if (card < 0)
	{
		return;
	}
	do
	{

		sprintf(cname, "hw:%d", card);
		if ((err = snd_ctl_open(&ctl, cname, 0)) < 0)
		{
			error("cannot open control for card %d: %s", card, snd_strerror(err));
			return;
		}
		device = -1;
		for (;;)
		{
			if ((err = snd_ctl_rawmidi_next_device(ctl, &device)) < 0)
			{
				error("cannot determine device number: %s", snd_strerror(err));
				break;
			}
			if (device < 0)
				break;
			snd_rawmidi_info_alloca(&info);
			snd_rawmidi_info_set_device(info, device);

			snd_rawmidi_info_set_stream(info, SND_RAWMIDI_STREAM_INPUT);
			err = snd_ctl_rawmidi_info(ctl, info);
			if (err >= 0)
				subs_in = snd_rawmidi_info_get_subdevices_count(info);
			else
				subs_in = 0;

			snd_rawmidi_info_set_stream(info, SND_RAWMIDI_STREAM_OUTPUT);
			err = snd_ctl_rawmidi_info(ctl, info);
			if (err >= 0)
				subs_out = snd_rawmidi_info_get_subdevices_count(info);
			else
				subs_out = 0;

			subs = subs_in > subs_out ? subs_in : subs_out;
			if (!subs)
				return;

			for (sub = 0; sub < subs; ++sub)
			{
				snd_rawmidi_info_set_stream(info, sub < subs_in ? SND_RAWMIDI_STREAM_INPUT : SND_RAWMIDI_STREAM_OUTPUT);
				snd_rawmidi_info_set_subdevice(info, sub);
				err = snd_ctl_rawmidi_info(ctl, info);
				if (err < 0)
				{
					/*error("cannot get rawmidi information %d:%d:%d: %s\n",
			      card, device, sub, snd_strerror(err));*/
					return;
				}
				name = snd_rawmidi_info_get_name(info);
				sub_name = snd_rawmidi_info_get_subdevice_name(info);
				if (sub == 0 && sub_name[0] == '\0')
				{
					printf("%c%c  hw:%d,%d    %s",
						   sub < subs_in ? 'I' : ' ',
						   sub < subs_out ? 'O' : ' ',
						   card, device, name);

					if (subs > 1)
						printf(" (%d subdevices)", subs);
					putchar('\n');
					sprintf(names[num++], "hw:%d,%d", card, device);
					break;
				}
				else
				{
					printf("%c%c  hw:%d,%d,%d  %s\n",
						   sub < subs_in ? 'I' : ' ',
						   sub < subs_out ? 'O' : ' ',
						   card, device, sub, sub_name);
					sprintf(names[num++], "hw:%d,%d,%d", card, device, sub);
				}
			}
		}
		snd_ctl_close(ctl);
		if ((err = snd_card_next(&card)) < 0)
		{
			break;
		}
	} while (card >= 0);

	return num;
}
