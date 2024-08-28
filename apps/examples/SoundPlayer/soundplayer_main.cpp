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
#include <iostream>
#include <functional>
#include <sys/stat.h>
#include <tinyara/init.h>
#include <media/FocusManager.h>
#include <media/MediaPlayer.h>
#include <media/FileInputDataSource.h>
#include <media/MediaUtils.h>
#include <string.h>
#include <debug.h>
#include <fcntl.h>
#include <dirent.h>
#include <limits.h>
#include <vector>
#define SOUND_PLAYER_LIVE_DATA
#if defined SOUND_PLAYER_LIVE_DATA
#include <sys/socket.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

using namespace media;
using namespace media::stream;

#if defined SOUND_PLAYER_LIVE_DATA
#define LIVE_DATA_PORT 5557
#define SOUND_PLAYER_PACKET_LENGTH 2048
#endif

#define DEFAULT_CONTENTS_PATH "/mnt"
#define DEFAULT_SAMPLERATE_TYPE AUDIO_SAMPLE_RATE_24000
#define DEFAULT_FORMAT_TYPE AUDIO_FORMAT_TYPE_S16_LE
#define DEFAULT_CHANNEL_NUM 1 //mono

static int gPlaybackFinished;
static int gAllTrackPlayed;

#if defined SOUND_PLAYER_LIVE_DATA
static int server_fd;
int player_client_fd;
int toShareData;
#endif

#if defined SOUND_PLAYER_LIVE_DATA
int live_player_data_share_setup() {
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

	if ((player_client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
		printf("failed to accept connection\n");
		return 0;
	}

	printf("Client connected, received acknowledgment\n");
	/* Connection with client established*/
	return 0;	
}
#endif

//***************************************************************************
// class : SoundPlayer
//***************************************************************************/

class SoundPlayer : public MediaPlayerObserverInterface,
					  public FocusChangeListener,
					  public std::enable_shared_from_this<SoundPlayer>
{
public:
	SoundPlayer() : volume(0), mNumContents(0), mPlayIndex(-1), mHasFocus(false), mSampleRate(DEFAULT_SAMPLERATE_TYPE), 
							mChannel(DEFAULT_CHANNEL_NUM) {};
	~SoundPlayer() {};
	bool init(char *argv[]);
	bool startPlayback(void);
	void onPlaybackStarted(MediaPlayer &mediaPlayer) override;
	void onPlaybackFinished(MediaPlayer &mediaPlayer) override;
	void onPlaybackError(MediaPlayer &mediaPlayer, player_error_t error) override;
	void onStartError(MediaPlayer &mediaPlayer, player_error_t error) override;
	void onStopError(MediaPlayer &mediaPlayer, player_error_t error) override;
	void onPauseError(MediaPlayer &mediaPlayer, player_error_t error) override;
	void onPlaybackPaused(MediaPlayer &mediaPlayer) override;
	void onAsyncPrepared(MediaPlayer &mediaPlayer, player_error_t error) override;
	void onFocusChange(int focusChange) override;

private:
	MediaPlayer mp;
	uint8_t volume;
	std::shared_ptr<FocusRequest> mFocusRequest;
	std::vector<std::string> mList;
	unsigned int mNumContents;
	unsigned int mPlayIndex;
	bool mHasFocus;
	unsigned int mSampleRate;
	unsigned int mChannel;

	void loadContents(const char *path);
};


void SoundPlayer::onPlaybackStarted(MediaPlayer &mediaPlayer)
{
	printf("onPlaybackStarted\n");
}

void SoundPlayer::onPlaybackFinished(MediaPlayer &mediaPlayer)
{
	printf("onPlaybackFinished playback index : %d\n", mPlayIndex);
#if defined SOUND_PLAYER_LIVE_DATA
		if (toShareData == 1) {
			int size = 0;
			int network_data = htonl(size);
			int ret = send(player_client_fd, (char *)&network_data, sizeof(int), 0);	
		}
#endif	
	mPlayIndex++;
	if (mPlayIndex == mNumContents) {
		gAllTrackPlayed = true;
		auto &focusManager = FocusManager::getFocusManager();
		focusManager.abandonFocus(mFocusRequest);
#if defined SOUND_PLAYER_LIVE_DATA
		if (toShareData == 1) {
			close(player_client_fd);
			shutdown(server_fd, SHUT_RDWR);
		}
#endif
		return;
	}
	printf("wait 3s until play next contents\n");
	mediaPlayer.unprepare();
	sleep(3);
	startPlayback();
}

void SoundPlayer::onPlaybackError(MediaPlayer &mediaPlayer, player_error_t error)
{
	printf("onPlaybackError error : %d\n", error);
}

void SoundPlayer::onStartError(MediaPlayer &mediaPlayer, player_error_t error)
{
	printf("onStartError error : %d\n", error);
}

void SoundPlayer::onPauseError(MediaPlayer &mediaPlayer, player_error_t error)
{
	printf("onPauseError error : %d\n", error);
}

void SoundPlayer::onStopError(MediaPlayer &mediaPlayer, player_error_t error)
{
	printf("onStopError error : %d\n", error);
}

void SoundPlayer::onPlaybackPaused(MediaPlayer &mediaPlayer)
{
	printf("onPlaybackPaused\n");
}

void SoundPlayer::onAsyncPrepared(MediaPlayer &mediaPlayer, player_error_t error)
{
	printf("onAsyncPrepared error : %d\n", error);
	if (error == PLAYER_ERROR_NONE) {
		mediaPlayer.start();
	} else {
		mediaPlayer.unprepare();
	}
}

void SoundPlayer::onFocusChange(int focusChange)
{
	player_result_t res;
	switch (focusChange) {
	case FOCUS_GAIN:
		mHasFocus = true;
		res = mp.prepare();
		if (res != PLAYER_OK) {
			printf("prepare failed res : %d\n", res);
			break;
		}
		res = mp.start();
		if (res != PLAYER_OK) {
			printf("start failed res : %d\n", res);
		}
		break;
	case FOCUS_LOSS:
		mHasFocus = false;
		if (gAllTrackPlayed) {
			printf("All Track played, Destroy Player\n");
			mp.unprepare();
			mp.destroy();
			gPlaybackFinished = true;
			return;
		}
		res = mp.pause();
		if (res != PLAYER_OK) {
			printf("pause failed res : %d\n", res);
		}
		break;
	default:
		break;
	}
}

bool SoundPlayer::init(char *argv[])
{
	struct stat st;
	int ret;
	char *path = argv[1];
	ret = stat(path, &st);
	if (ret != OK) {
		printf("invalid path : %s\n", path);
		return false;
	}
	if (S_ISDIR(st.st_mode)) {
		loadContents(path);
	} else {
		std::string s = path;
		mList.push_back(s);
	}

	mNumContents = mList.size();
	if (mNumContents > 0) {
		mPlayIndex = 0;
	}
	printf("Show Track List!! mNumContents : %d mPlayIndex : %d\n", mNumContents, mPlayIndex);
	for (int i = 0; i != (int)mList.size(); i++) {
		printf("path : %s\n", mList.at(i).c_str());
	}
	player_result_t res = mp.create();
	if (res != PLAYER_OK) {
		printf("MediaPlayer create failed res : %d\n", res);
		return false;
	}
	mp.setObserver(shared_from_this());

	volume = atoi(argv[2]);
	uint8_t cur_vol;
	mp.getVolume(&cur_vol);
	printf("Current volume : %d new Volume : %d\n", cur_vol, volume);
	mp.setVolume(volume);
	stream_info_t *info;
	stream_info_create(STREAM_TYPE_MEDIA, &info);
	auto deleter = [](stream_info_t *ptr) { stream_info_destroy(ptr); };
	auto stream_info = std::shared_ptr<stream_info_t>(info, deleter);
	mFocusRequest = FocusRequest::Builder()
						.setStreamInfo(stream_info)
						.setFocusChangeListener(shared_from_this())
						.build();

	mSampleRate = atoi(argv[3]);
	gAllTrackPlayed = false;
	mChannel = atoi(argv[4]);
	toShareData = atoi(argv[5]);
	if (toShareData) {
		live_player_data_share_setup();
		printf("Number of files to play: %d\n", mNumContents);
		int network_data = htonl(mNumContents);
		int ret = send(player_client_fd, (char *)&network_data, sizeof(int), 0);	
	}
	return true;
}

bool SoundPlayer::startPlayback(void)
{
	player_result_t res;
	std::string s = mList.at(mPlayIndex);
	printf("startPlayback... playIndex : %d path : %s\n", mPlayIndex, s.c_str());
	auto source = std::move(std::unique_ptr<FileInputDataSource>(new FileInputDataSource((const std::string)s)));
	source->setSampleRate(mSampleRate);
	source->setChannels(mChannel);
	source->setPcmFormat(DEFAULT_FORMAT_TYPE);
	res = mp.setDataSource(std::move(source));
	if (res != PLAYER_OK) {
		printf("set Data source failed. res : %d\n", res);
		return false;
	}
	if (!mHasFocus) {
		auto &focusManager = FocusManager::getFocusManager();
		focusManager.requestFocus(mFocusRequest);
	} else {
		res = mp.prepare();
		if (res != PLAYER_OK) {
			printf("prepare failed res : %d\n", res);
			return false;
		}
		res = mp.start();
		if (res != PLAYER_OK) {
			printf("start failed res : %d\n", res);
			return false;
		}
	}
	return true;
}

/* list all files in the directory */
void SoundPlayer::loadContents(const char *dirpath)
{
	DIR *dirp = opendir(dirpath);
	if (!dirp) {
		printf("Failed to open directory %s\n", dirpath);
		return;
	}

	struct dirent *entryp;
	char path[CONFIG_PATH_MAX];

	while ((entryp = readdir(dirp)) != NULL) {
		if ((strcmp(".", entryp->d_name) == 0) || (strcmp("..", entryp->d_name) == 0)) {
			continue;
		}

		snprintf(path, CONFIG_PATH_MAX, "%s/%s", dirpath, entryp->d_name);
		if (DIRENT_ISDIRECTORY(entryp->d_type)) {
			loadContents(path);
		} else {
			/* this entry is a file, add it to list. */
			std::string s = path;
			audio_type_t type = utils::getAudioTypeFromPath(s);
			if (type != AUDIO_TYPE_INVALID) {
				mList.push_back(s);
			}
		}
	}
	closedir(dirp);
}

extern "C" {
int soundplayer_main(int argc, char *argv[])
{
	auto player = std::shared_ptr<SoundPlayer>(new SoundPlayer());
	if (argc != 6) {
		printf("invalid input\n");
		return -1;
	}
	gPlaybackFinished = false;
	if (!player->init(argv)) {
		return -1;
	}
	if (!player->startPlayback()) {
		return -1;
	}
	while (!gPlaybackFinished) {
		sleep(1);
	}
	printf("terminate application\n");
	return 0;
}
}
