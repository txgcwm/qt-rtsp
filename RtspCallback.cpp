#include <iostream>

#include "RtspCallback.h"
#include "StreamState.h"
#include "RtspClient.h"
#include "DummySink.h"


#define REQUEST_STREAMING_OVER_TCP False

// A function that outputs a string that identifies each stream (for debugging output).  Modify this if you wish:
UsageEnvironment& operator<<(UsageEnvironment& env, const RTSPClient& rtspClient)
{
    return env << "[URL:\"" << rtspClient.url() << "\"]: ";
}

// A function that outputs a string that identifies each subsession (for debugging output).  Modify this if you wish:
UsageEnvironment& operator<<(UsageEnvironment& env, const MediaSubsession& subsession)
{
    return env << subsession.mediumName() << "/" << subsession.codecName();
}

void continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString)
{
    do {
        UsageEnvironment& env = rtspClient->envir(); // alias
        StreamClientState& scs = ((CustomRTSPClient*)rtspClient)->getStreamClientState(); // alias

        if (resultCode != 0) {
            env << *rtspClient << "Failed to get a SDP description: " << resultString << "\n";
            delete[] resultString;
            break;
        }

        char* const sdpDescription = resultString;
        env << *rtspClient << "Got a SDP description:\n" << sdpDescription << "\n";

        // Create a media session object from this SDP description:
        scs.session = MediaSession::createNew(env, sdpDescription);
        delete[] sdpDescription; // because we don't need it anymore
        if (scs.session == nullptr) {
            env << *rtspClient << "Failed to create a MediaSession object from the SDP description: " << env.getResultMsg() << "\n";
            break;
        } else if (!scs.session->hasSubsessions()) {
            env << *rtspClient << "This session has no media subsessions (i.e., no \"m=\" lines)\n";
            break;
        }

        // Then, create and set up our data source objects for the session.  We do this by iterating over the session's 'subsessions',
        // calling "MediaSubsession::initiate()", and then sending a RTSP "SETUP" command, on each one.
        // (Each 'subsession' will have its own data source.)
        scs.iter = new MediaSubsessionIterator(*scs.session);
        setupNextSubsession(rtspClient);
        return;
    } while (0);

    // An unrecoverable error occurred with this stream.
    shutdownStream(rtspClient);
}

void continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString)
{
    do {
        UsageEnvironment& env = rtspClient->envir(); // alias
        StreamClientState& streamClientState = ((CustomRTSPClient*)rtspClient)->getStreamClientState(); // alias

        if (resultCode != 0) {
            env << *rtspClient << "Failed to set up the \"" << *streamClientState.subsession << "\" subsession: " << resultString << "\n";
            break;
        }

        env << *rtspClient << "Set up the \"" << *streamClientState.subsession << "\" subsession (";
        if (streamClientState.subsession->rtcpIsMuxed()) {
            env << "client port " << streamClientState.subsession->clientPortNum();
        } else {
            env << "client ports " << streamClientState.subsession->clientPortNum() << "-" << streamClientState.subsession->clientPortNum()+1;
        }
        env << ")\n";

        const char *sprop = streamClientState.subsession->fmtp_spropparametersets();
        uint8_t const* sps = nullptr;
        unsigned spsSize = 0;
        uint8_t const* pps = nullptr;
        unsigned ppsSize = 0;

        if (sprop != nullptr) {
            unsigned numSPropRecords;
            SPropRecord* sPropRecords = parseSPropParameterSets(sprop, numSPropRecords);
            for (unsigned i = 0; i < numSPropRecords; ++i) {
                if (sPropRecords[i].sPropLength == 0) continue; // bad data
                u_int8_t nal_unit_type = (sPropRecords[i].sPropBytes[0])&0x1F;
                if (nal_unit_type == 7/*SPS*/) {
                    sps = sPropRecords[i].sPropBytes;
                    spsSize = sPropRecords[i].sPropLength;
                } else if (nal_unit_type == 8/*PPS*/) {
                    pps = sPropRecords[i].sPropBytes;
                    ppsSize = sPropRecords[i].sPropLength;
                }
            }
        }

        // Having successfully setup the subsession, create a data sink for it, and call "startPlaying()" on it.
        // (This will prepare the data sink to receive data; the actual flow of data from the client won't start happening until later,
        // after we've sent a RTSP "PLAY" command.)

        streamClientState.subsession->sink = DummySink::createNew(((CustomRTSPClient*)rtspClient)->getSession(), env, *streamClientState.subsession, rtspClient->url());
        // perhaps use your own custom "MediaSink" subclass instead
        if (streamClientState.subsession->sink == nullptr) {
            env << *rtspClient << "Failed to create a data sink for the \"" << *streamClientState.subsession
                << "\" subsession: " << env.getResultMsg() << "\n";
            break;
        }

        env << *rtspClient << "Created a data sink for the \"" << *streamClientState.subsession << "\" subsession\n";
        streamClientState.subsession->miscPtr = rtspClient; // a hack to let subsession handler functions get the "RTSPClient" from the subsession

        if (sps != nullptr) {
            ((DummySink *)streamClientState.subsession->sink)->setSprop(sps, spsSize);
        }
        if (pps != nullptr) {
            ((DummySink *)streamClientState.subsession->sink)->setSprop(pps, ppsSize);
        }

        streamClientState.subsession->sink->startPlaying(*(streamClientState.subsession->readSource()),
                                           subsessionAfterPlaying, streamClientState.subsession);

        // Also set a handler to be called if a RTCP "BYE" arrives for this subsession:
        if (streamClientState.subsession->rtcpInstance() != NULL) {
            streamClientState.subsession->rtcpInstance()->setByeHandler(subsessionByeHandler, streamClientState.subsession);
        }
    } while (0);
    delete[] resultString;

    // Set up the next subsession, if any:
    setupNextSubsession(rtspClient);
}

void continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString)
{
    Boolean success = False;

    do {
        UsageEnvironment& env = rtspClient->envir(); // alias
        StreamClientState& scs = ((CustomRTSPClient*)rtspClient)->getStreamClientState(); // alias

        if (resultCode != 0) {
            env << *rtspClient << "Failed to start playing session: " << resultString << "\n";
            break;
        }

        // Set a timer to be handled at the end of the stream's expected duration (if the stream does not already signal its end
        // using a RTCP "BYE").  This is optional.  If, instead, you want to keep the stream active - e.g., so you can later
        // 'seek' back within it and do another RTSP "PLAY" - then you can omit this code.
        // (Alternatively, if you don't want to receive the entire stream, you could set this timer for some shorter value.)
        if (scs.duration > 0) {
            unsigned const delaySlop = 2; // number of seconds extra to delay, after the stream's expected duration.  (This is optional.)
            scs.duration += delaySlop;
            unsigned uSecsToDelay = (unsigned)(scs.duration*1000000);
            scs.streamTimerTask = env.taskScheduler().scheduleDelayedTask(uSecsToDelay, (TaskFunc*)streamTimerHandler, rtspClient);
        }

        env << *rtspClient << "Started playing session";
        if (scs.duration > 0) {
            env << " (for up to " << scs.duration << " seconds)";
        }
        env << "...\n";

        success = True;
    } while (0);
    delete[] resultString;

    if (!success) {
        // An unrecoverable error occurred with this stream.
        shutdownStream(rtspClient);
    }
}

void setupNextSubsession(RTSPClient* rtspClient)
{
    UsageEnvironment& env = rtspClient->envir(); // alias
    StreamClientState& scs = ((CustomRTSPClient*)rtspClient)->getStreamClientState(); // alias

    scs.subsession = scs.iter->next();
    if (scs.subsession != nullptr) {
        if (!scs.subsession->initiate()) {
            env << *rtspClient << "Failed to initiate the \"" << *scs.subsession << "\" subsession: " << env.getResultMsg() << "\n";
            setupNextSubsession(rtspClient); // give up on this subsession; go to the next one
        } else {
            env << *rtspClient << "Initiated the \"" << *scs.subsession << "\" subsession (";
            if (scs.subsession->rtcpIsMuxed()) {
                env << "client port " << scs.subsession->clientPortNum();
            } else {
                env << "client ports " << scs.subsession->clientPortNum() << "-" << scs.subsession->clientPortNum()+1;
            }
            env << ")\n";

            // Continue setting up this subsession, by sending a RTSP "SETUP" command:
            rtspClient->sendSetupCommand(*scs.subsession, continueAfterSETUP, False, REQUEST_STREAMING_OVER_TCP);
        }
        return;
    }

    // We've finished setting up all of the subsessions.  Now, send a RTSP "PLAY" command to start the streaming:
    if (scs.session->absStartTime() != nullptr) {
        // Special case: The stream is indexed by 'absolute' time, so send an appropriate "PLAY" command:
        rtspClient->sendPlayCommand(*scs.session, continueAfterPLAY, scs.session->absStartTime(), scs.session->absEndTime());
    } else {
        scs.duration = scs.session->playEndTime() - scs.session->playStartTime();
        rtspClient->sendPlayCommand(*scs.session, continueAfterPLAY);
    }
}

// Implementation of the other event handlers:

void subsessionAfterPlaying(void* clientData)
{
    MediaSubsession* subsession = (MediaSubsession*)clientData;
    RTSPClient* rtspClient = (RTSPClient*)(subsession->miscPtr);

    // Begin by closing this subsession's stream:
    Medium::close(subsession->sink);
    subsession->sink = nullptr;

    // Next, check whether *all* subsessions' streams have now been closed:
    MediaSession& session = subsession->parentSession();
    MediaSubsessionIterator iter(session);
    while ((subsession = iter.next()) != nullptr) {
        if (subsession->sink != nullptr)
            return; // this subsession is still active
    }

    // All subsessions' streams have now been closed, so shutdown the client:
    shutdownStream(rtspClient);

    return;
}

void subsessionByeHandler(void* clientData)
{
    MediaSubsession* subsession = (MediaSubsession*)clientData;
    RTSPClient* rtspClient = (RTSPClient*)subsession->miscPtr;
    UsageEnvironment& env = rtspClient->envir(); // alias

    env << *rtspClient << "Received RTCP \"BYE\" on \"" << *subsession << "\" subsession\n";

    // Now act as if the subsession had closed:
    subsessionAfterPlaying(subsession);
}

void streamTimerHandler(void* clientData)
{
    CustomRTSPClient* rtspClient = (CustomRTSPClient*)clientData;
    StreamClientState& scs = rtspClient->getStreamClientState(); // alias

    scs.streamTimerTask = nullptr;

    shutdownStream(rtspClient);  // Shut down the stream

    return;
}

void shutdownStream(RTSPClient* rtspClient)
{
//    UsageEnvironment& env = rtspClient->envir(); // alias
    StreamClientState& scs = ((CustomRTSPClient*)rtspClient)->getStreamClientState(); // alias

    // First, check whether any subsessions have still to be closed:
    if (scs.session != nullptr) {
        Boolean someSubsessionsWereActive = False;
        MediaSubsessionIterator iter(*scs.session);
        MediaSubsession* subsession;

        while ((subsession = iter.next()) != nullptr) {
            if (subsession->sink != nullptr) {
                Medium::close(subsession->sink);
                subsession->sink = nullptr;

                if (subsession->rtcpInstance() != nullptr) {
                    subsession->rtcpInstance()->setByeHandler(nullptr, nullptr); // in case the server sends a RTCP "BYE" while handling "TEARDOWN"
                }

                someSubsessionsWereActive = True;
            }
        }

        if (someSubsessionsWereActive) {
            // Send a RTSP "TEARDOWN" command, to tell the server to shutdown the stream.
            // Don't bother handling the response to the "TEARDOWN".
            rtspClient->sendTeardownCommand(*scs.session, nullptr);
        }
    }

    std::cout << "Closing the stream.\n";
    Medium::close(rtspClient);

    return;
}
