/****************************************************************************
 *
 * Copyright 2024 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/
//***************************************************************************
// Included Files
//***************************************************************************

#include <tinyara/config.h>
#include <stdio.h>
#include <string.h>
#include <media/MediaRecorder.h>
#include <media/MediaRecorderObserverInterface.h>
#include <media/FileOutputDataSource.h>
#include <media/BufferOutputDataSource.h>
#define SOUND_RECORDER_LIVE_DATA
#if defined SOUND_RECORDER_LIVE_DATA
#include <sys/socket.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

using namespace media;
using namespace media::stream;

#define SAMPLE_RATE 16000
#define NUMBER_OF_CHANNELS 1
#define RECORD_DURATION_IN_SECONDS 7
#define VOLUME 8
#define MNT_FILE_PATH "/mnt/soundrecord"
#define FILE_NAME_SIZE 30
#if defined SOUND_RECORDER_LIVE_DATA
#define LIVE_DATA_PORT 5556
#define SOUND_RECORDER_PACKET_LENGTH 2048
#endif

static const int TEST_DATASOURCE_TYPE_BUFFER = 0;
static const int TEST_DATASOURCE_TYPE_FILE = 1;
const char* datasourcetype[] = {"buffer", "file"};

static const int TEST_MEDIATYPE_PCM = 0;
static const int TEST_MEDIATYPE_OPUS = 1;
static const int TEST_MEDIATYPE_WAVE = 2;
const char* typefile[] = {"pcm", "opus", "wav"};

static const int TEST_REALTIME_TRANSFER_OFF = 0;
static const int TEST_REALTIME_TRANSFER_ON = 1;
const char* typeRealTimeTransfer[] = {"off", "on"};

static char filePath[FILE_NAME_SIZE];
static FILE *gPCMFile = NULL;
static int file_no = 0;
static bool changedSetting = false;
static int sampRate;
static int nChannels;
static int recordTime;
static uint8_t volume;
static uint8_t maxVolume;
static int mMediaType;
static int mDataSourceType;
static int firstuse = true;
static int newVolume;
static int isRecording = false;
static int isRealTime = 1;
#if defined SOUND_RECORDER_LIVE_DATA
static int server_fd;
static int client_fd;
#endif

media::MediaRecorder soundRec;

#if defined SOUND_RECORDER_LIVE_DATA
int live_data_share_setup() {
	struct sockaddr_in address;
	int opt = 1;
	int addrlen = sizeof(address);

	/* Creating socket file descriptor */
	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("failed to fetch a socket fd\n");
		return 0;
	}

	/* Forcefully attaching socket to the port */
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
		printf("setting socket options failed\n");
		return 0;
	}

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	address.sin_port = htons(LIVE_DATA_PORT);
	if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
		printf("binding socket failed\n");
		return 0;
	}

	/* lets wait for client to connect */
	printf("Waiting for client to connect............\n");
	if (listen(server_fd, 3) < 0) {
		printf("listening on socket failed\n");
		return 0;
	}

	if ((client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
		printf("failed to accept connection\n");
		return 0;
	}

	printf("Client connected, received acknowledgment\n");
	/* Connection with client established*/
	return 0;	
}
#endif

class SoundRecorderObserver : public media::MediaRecorderObserverInterface, public std::enable_shared_from_this<SoundRecorderObserver>
{
	void onRecordStarted(media::MediaRecorder &mediaRecorder) override
	{
		printf("##################################\n");
		printf("####     onRecordStarted      ####\n");
		printf("##################################\n");
		if (!isRealTime) {
			if (mDataSourceType == TEST_DATASOURCE_TYPE_BUFFER) {
				gPCMFile = fopen(filePath, "w");
				if (gPCMFile == NULL) {
					std::cout << "FILE OPEN FAILED" << std::endl;
					return;
				}
			}
		}
	}
	void onRecordPaused(media::MediaRecorder &mediaRecorder) override
	{
		printf("##################################\n");
		printf("####      onRecordPaused      ####\n");
		printf("##################################\n");
	}
	void onRecordFinished(media::MediaRecorder &mediaRecorder) override
	{
		printf("##################################\n");
		printf("####      onRecordFinished    ####\n");
		printf("##################################\n");
        if (mDataSourceType == TEST_DATASOURCE_TYPE_BUFFER && isRealTime == 0 && gPCMFile != NULL) {
			fclose(gPCMFile);
		}
		soundRec.unprepare();
		soundRec.destroy();
		isRecording = false;
#if defined SOUND_RECORDER_LIVE_DATA
		if (isRealTime == 1) {
			int size = 0;
			int network_data = htonl(size);
			int ret = send(client_fd, (char *)&network_data, sizeof(int), 0);
			close(client_fd);
			shutdown(server_fd, SHUT_RDWR);
		}
#endif
		file_no++;
	}
	void onRecordStartError(media::MediaRecorder &mediaRecorder, media::recorder_error_t errCode) override
	{
		printf("#### onRecordStartError!! errCode : %d\n", errCode);
	}
	void onRecordPauseError(media::MediaRecorder &mediaRecorder, media::recorder_error_t errCode) override
	{
		printf("#### onRecordPauseError!! errCode : %d\n", errCode);
	}
	void onRecordStopError(media::MediaRecorder &mediaRecorder, media::recorder_error_t errCode) override
	{
		printf("#### onRecordStopError!! errCode : %d\n", errCode);
	}
	void onRecordBufferDataReached(media::MediaRecorder &mediaRecorder, std::shared_ptr<unsigned char> data, size_t size) override
	{
#if defined SOUND_RECORDER_LIVE_DATA
		if (isRealTime == 1) {
			printf("[onRBDR] send data size %d\n", size);
			int network_data = htonl(size);
			int ret = send(client_fd, (char *)&network_data, sizeof(int), 0);
			size_t sent_bytes = send(client_fd, (char *)data.get(), size, 0);
			printf("Sent data %d\n", sent_bytes);
		}
#endif
		if (isRealTime == 0) {
			if (gPCMFile != NULL) {
				int sz_written = fwrite(data.get(), sizeof(unsigned char), size, gPCMFile);
				printf("Audio data dump write operation done, size: %d\n", sz_written);
			}
		}
	}
};

void set_volume()
{
    if (soundRec.setVolume(newVolume) != RECORDER_ERROR_NONE) {
		printf("MediaRecorder::setVolume failed\n");
	} else {
    	volume = newVolume;
	}
}

void get_volume()
{
	if (soundRec.getVolume(&volume) != RECORDER_ERROR_NONE) {
        printf("MediaRecorder::getVolume failed\n");
    } else {
        printf("SoundRecorder Current Volume is %d\n", volume);
    }
}

void get_max_volume()
{
	if (soundRec.getMaxVolume(&maxVolume) != RECORDER_ERROR_NONE) {
		printf("MediaRecorder::getMaxVolume failed\n");
	} else {
        printf("SundRecorder Max Volume is %d\n", maxVolume);
	}
}

bool startRecord(void)
{
	media::recorder_result_t mret = soundRec.create();
	if (mret == media::RECORDER_OK) {
		printf("#### [MR] create succeeded.\n");
	} else {
		printf("#### [MR] create failed.\n");
		return false;
	}

	if (!isRealTime) {
		if (mMediaType == TEST_MEDIATYPE_PCM) {
			snprintf(filePath, FILE_NAME_SIZE, "%s%d%s", MNT_FILE_PATH, file_no, ".pcm");
		} else if (mMediaType == TEST_MEDIATYPE_OPUS) {
			snprintf(filePath, FILE_NAME_SIZE, "%s%d%s", MNT_FILE_PATH, file_no, ".opus");
		} else if (mMediaType == TEST_MEDIATYPE_WAVE) {
			snprintf(filePath, FILE_NAME_SIZE, "%s%d%s", MNT_FILE_PATH, file_no, ".wav");
		}
		printf("File path for recording is %s\n", filePath);
	}

    if (mDataSourceType == TEST_DATASOURCE_TYPE_FILE) {
		soundRec.setDataSource(std::unique_ptr<FileOutputDataSource>(
			new FileOutputDataSource(nChannels, sampRate, media::AUDIO_FORMAT_TYPE_S16_LE, filePath)));
	} else if (mDataSourceType == TEST_DATASOURCE_TYPE_BUFFER) {
		soundRec.setDataSource(std::unique_ptr<BufferOutputDataSource>(
            new BufferOutputDataSource(nChannels, sampRate, media::AUDIO_FORMAT_TYPE_S16_LE)));
	}
	if (mret == media::RECORDER_OK) {
		printf("#### [MR] setDataSource succeeded.\n");
	} else {
		printf("#### [MR] setDataSource failed.\n");
		return false;
	}

	mret = soundRec.setObserver(std::make_shared<SoundRecorderObserver>());
	if (mret == media::RECORDER_OK) {
		printf("#### [MR] setObserver succeeded.\n");
	} else {
		printf("#### [MR] setObserver failed.\n");
		return false;
	}

	if (soundRec.setDuration(recordTime) == RECORDER_ERROR_NONE && soundRec.prepare() == RECORDER_ERROR_NONE) {
		printf("#### [MR] prepare succeeded.\n");
	} else {
		printf("#### [MR] prepare failed.\n");
		return false;
	}

	if (firstuse) {
		firstuse = false;
		get_max_volume();
	}
	get_volume();
	set_volume();
	mret = soundRec.start();
	if (mret == media::RECORDER_OK) {
		printf("#### [MR] start succeeded.\n");
		return true;
	} else {
		printf("#### [MR] start failed.\n");
		return false;
	}
}

void set_default_setting() {
    sampRate = SAMPLE_RATE;
    nChannels = NUMBER_OF_CHANNELS;
    recordTime = RECORD_DURATION_IN_SECONDS;
    volume = VOLUME;
    mDataSourceType = TEST_DATASOURCE_TYPE_BUFFER;
	mMediaType = TEST_MEDIATYPE_PCM;
	isRealTime = 1;
}

void set_recorder_settings(int argc, char *argv[])
{
	if (strncmp(argv[2], "samprate", 9) == 0) {
		sampRate = atoi(argv[3]);
		printf("Soundrecorder samplerate is set to %d\n", sampRate, "Hz\n");
	} else if (strncmp(argv[2], "channels", 9) == 0) {
		nChannels = atoi(argv[3]);
		printf("Soundrecorder no of channels is set to %d\n", nChannels);
	} else if (strncmp(argv[2], "duration", 9) == 0) {
		recordTime = atoi(argv[3]);
		printf("Soundrecorder duration is set to %d seconds\n", recordTime);
	} else if (strncmp(argv[2], "volume", 7) == 0) {
		newVolume = atoi(argv[3]);
		printf("Soundrecorder volume will be set to %d from next record.\n", newVolume);
	} else if (strncmp(argv[2], "source", 7) == 0) {
		mDataSourceType = atoi(argv[3]);
		printf("Set source type to %s\n", datasourcetype[mDataSourceType]);
	} else if (strncmp(argv[2], "file", 5) == 0) {
		mMediaType = atoi(argv[3]);
		printf("Set file type to %s\n", typefile[mMediaType]);
	} else if (strncmp(argv[2], "realtime", 9) == 0) {
		isRealTime = atoi(argv[3]);
		printf("Set realtime recording to %s\n", typeRealTimeTransfer[isRealTime]);
	} else {
		printf("Invalid option to set recorder.\n");
	}
}

void get_full_settings()
{
    printf("SoundRecorder Settings:\n");
    printf("SoundRecorder SampleRate : %d\n", sampRate);
    printf("SoundRecorder nChannels : %d\n", nChannels);
    printf("SoundRecorder Duration : %d\n", recordTime);
	if(!firstuse) {
		printf("Soundrecorder Volume : %d\n", volume);
		printf("SoundRecorder MaxVolume : %d\n", maxVolume);
	}
    printf("SoundRecorder DataSourceType : %s\n", datasourcetype[mDataSourceType]);
	printf("Media Type Selected is %s\n", typefile[mMediaType]);
	printf("Realtime Transfer Supported : %s\n", typeRealTimeTransfer[isRealTime]);
}

void get_recorder_settings(int argc, char *argv[])
{
    if (argc == 2) {
        get_full_settings();
    } else {
        if (strncmp(argv[2], "samprate", 9) == 0) {
            printf("SoundRecorder SampleRate : %d\n", sampRate);
        } else if (strncmp(argv[2], "channels", 9) == 0) {
            printf("SoundRecorder nChannels : %d\n", nChannels);
        } else if (strncmp(argv[2], "duration", 9) == 0) {
            printf("SoundRecorder Duration : %d\n", recordTime);
        } else if (strncmp(argv[2], "volume", 7) == 0) {
			if(!firstuse) {
				printf("Soundrecorder Volume during last record: %d\n", volume);	
			} else {
				printf("Require first recording to check to volume\n");
			}
        } else if (strncmp(argv[2], "maxvolume", 10) == 0) {
			if(!firstuse) {
				printf("Soundrecorder Max Volume : %d\n", maxVolume);	
			} else {
				printf("Require first recording to check to max volume\n");
			}
        } else if (strncmp(argv[2], "source", 7) == 0) {
            printf("Data Source Selected is %s\n", datasourcetype[mDataSourceType]);
        } else if (strncmp(argv[2], "file", 5) == 0) {
			printf("Media Type Selected is %d\n", typefile[mMediaType]);
		} else if (strncmp(argv[2], "realtime", 9)) {
			printf("Recorder data transfer type is %s\n", typeRealTimeTransfer[isRealTime]);
		} else {
            printf("Invalid option to get recorder setting.\n");
        }
    }
}

void print_usage()
{
	printf("\nsoundrecorder Usage:[Description : command]\n");
	printf("1. Setting Recorder Settings : soundrecorder set <key> <value>\n");
	printf("2. Getting Recorder Settings : soundrecorder get <key>\n");
	printf("3. Start Rercording : soundrecorder record\n");
	printf("4. Stop Recording : soundrecorder stop\n");
	printf("5. Print Usage of soundrecorder: soundrecorder\n\n");

	printf("SoundRecorder has following settings(keys[fullname] : Examples):\n");
	printf("1. samprate [Sample Rate]: 8000, 16000, 32000, 44100, 48000\n");
	printf("2. channels [Channels]: 1, 2\n");
	printf("3. duration [Record Duration]: 0[infinite record], 10, 20, 30, 60\n");
	printf("4. volume [Volume]: 0~10\n");
	printf("5. source [DataSource]: 0:buffer, 1:file\n");
	printf("6. file [File Extension]: 0 : pcm, 1 : opus, 2 : wav\n");
	printf("7. maxvolume [Maximum Volume] We can only get this and not set this.\n");
	printf("8. realtime [Realtime Data Share]: 0:off, 1:on\n");
	printf("\n");
}

void parse_soundrecorder_options(int argc, char *argv[])
{
    if (argc == 1) {
		print_usage();
    } else {
        if (strncmp(argv[1], "set", 4) == 0) {
			if (argc != 4) {
				printf("Invalid set Command. Use soundrecorder for usage.\n");
				return;
			}
            printf("Let's set some settings:\n");
			changedSetting = true;
            set_recorder_settings(argc, argv);
        } else if (strncmp(argv[1], "get", 4) == 0) {
			if (argc > 3) {
				printf("Too much arguments for get. Use soundrecorder for usage.\n");
				return;
			}
			printf("Let's get some settings:\n");
			get_recorder_settings(argc, argv);
		} else if (strncmp(argv[1], "record", 7) == 0) {
			if (argc > 2) {
				printf("Invalid Record Command. Use soundrecorder for usage.\n");
				return;
			}
			if (isRealTime) {
				live_data_share_setup();
			}
			printf("[parse_soundrecorder] Start Recording\n");
			bool startStatus = startRecord();
			if (startStatus == true) {
				isRecording = true;
			}
        } else if (strncmp(argv[1], "stop", 5) == 0) {
			if (argc > 2) {
				printf("Invalid Stop Record Command. Use soundrecorder for usage.\n");
				return;
			}
			printf("Stop Recording\n");
			soundRec.stop();
		} else {
            printf("Not valid command. Use soundrecorder for usage.\n");
        }
    }
}

extern "C" {
int soundrecorder_main(int argc, char *argv[])
{
	printf("SoundRecorder Application\n");
	if (!changedSetting) {
		set_default_setting();
	}

    parse_soundrecorder_options(argc, argv);
	while(isRecording) {
		sleep(1);
	}

    printf("Graceful Exit\n");
	return 0;
}
}
