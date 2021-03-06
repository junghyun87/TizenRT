/* ****************************************************************
 *
 * Copyright 2018 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ******************************************************************/

#include <debug.h>

#include "PlayerWorker.h"
#include "MediaPlayerImpl.h"
#include "Decoder.h"
#include "audio/audio_manager.h"

using namespace std;

namespace media {
PlayerWorker::PlayerWorker() : mCurPlayer(nullptr)
{
}

PlayerWorker::~PlayerWorker()
{
}

PlayerWorker& PlayerWorker::getWorker()
{
	static PlayerWorker worker;
	return worker;
}

int PlayerWorker::entry()
{
	while (mIsRunning) {
		if (mCurPlayer && (mCurPlayer->mCurState == PLAYER_STATE_PLAYING)) {
			shared_ptr<Decoder> mDecoder = mCurPlayer->mInputDataSource->getDecoder();
			if (mDecoder) {
				size_t num_read = mCurPlayer->mInputDataSource->read(mCurPlayer->mBuffer,
					((size_t)mCurPlayer->mBufSize < mDecoder->getAvailSpace()) ?
						mCurPlayer->mBufSize : mDecoder->getAvailSpace());
				medvdbg("MediaPlayer Worker(has mDecoder) : num_read = %d\n", num_read);

				/* Todo: Below code has an issue about file EOF.
				 *       Should be fixed after discuss.
				 */
				if (num_read == 0 && mDecoder->empty()) {
					mCurPlayer->notifyObserver(PLAYER_OBSERVER_COMMAND_FINISHIED);
					mCurPlayer->stop();
				} else {
					/* Todo: Currently, below function is working correctly.
					 *       Because the read size cannot be larger than remain data space.
					 *       But this isn't beautiful logic. Need to rearrange and modify the logic.
					 */
					mDecoder->pushData(mCurPlayer->mBuffer, num_read);

					size_t size = 0;
					unsigned int sampleRate = 0;
					unsigned short channels = 0;
					while (mDecoder->getFrame(mCurPlayer->mBuffer, &size, &sampleRate, &channels)) {
						medvdbg("Decoded Frame: %d bytes %d %d\n", size, sampleRate, channels);
						mCurPlayer->mInputDataSource->setSampleRate(sampleRate);
						mCurPlayer->mInputDataSource->setChannels(channels);
						mCurPlayer->playback(size);
					}
				}
			} else {
				size_t num_read = mCurPlayer->mInputDataSource->read(
					mCurPlayer->mBuffer,
					((int)mCurPlayer->mBufSize < get_output_frames_byte_size(get_output_frame_count())) ?
						mCurPlayer->mBufSize : get_output_frames_byte_size(get_output_frame_count()));
				medvdbg("MediaPlayer Worker : num_read = %d\n", num_read);

				if (num_read > 0) {
					mCurPlayer->playback(num_read);
				} else {
					mCurPlayer->notifyObserver(PLAYER_OBSERVER_COMMAND_FINISHIED);
					mCurPlayer->stop();
				}
			}

			if (mWorkerQueue.isEmpty()) {
				continue;
			}
		}

		std::function<void()> run = mWorkerQueue.deQueue();
		if (run != nullptr) {
			run();
		}
	}

	return 0;
}

void PlayerWorker::setPlayer(std::shared_ptr<MediaPlayerImpl> player)
{
	mCurPlayer = player;
}

std::shared_ptr<MediaPlayerImpl> PlayerWorker::getPlayer()
{
	return mCurPlayer;
}

} // namespace media
