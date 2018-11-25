/* $Id: pjsua2_demo.cpp 5717 2017-12-19 01:45:37Z nanang $ */
/*
 * Copyright (C) 2008-2013 Teluu Inc. (http://www.teluu.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <pjsua2.hpp>
#include <iostream>
#include <pj/file_access.h>

#define THIS_FILE 	"pjsua2_demo.cpp"

using namespace pj;
using namespace std;

class MyAccount;

class MyCall : public Call
{
private:
    MyAccount *myAcc;
    AudioMediaPlayer player;
    AudioMediaRecorder recorder;
    AudioMediaRecorder recorderVerify;

public:
    MyCall(Account &acc, int call_id = PJSUA_INVALID_ID)
    : Call(acc, call_id)
    {
        myAcc = (MyAccount *)&acc;
	recorder.createRecorder("in.wav");
    }
    
    virtual void onCallState(OnCallStateParam &prm);
    void onCallMediaState(OnCallMediaStateParam &prm);


};

class MyAccount : public Account
{
public:
    std::vector<Call *> calls;
    bool active;
    
public:
    MyAccount():active(true)
    {}

    ~MyAccount()
    {
        std::cout << "*** Account is being deleted: No of calls="
                  << calls.size() << std::endl;
    }
    
    void removeCall(Call *call)
    {
        for (std::vector<Call *>::iterator it = calls.begin();
             it != calls.end(); ++it)
        {
            if (*it == call) {
                calls.erase(it);
                break;
            }
        }
    }

    virtual void onRegState(OnRegStateParam &prm)
    {
	AccountInfo ai = getInfo();
	std::cout << (ai.regIsActive? "*** Register: code=" : "*** Unregister: code=")
		  << prm.code << std::endl;
    }
    
    virtual void onIncomingCall(OnIncomingCallParam &iprm)
    {
        Call *call = new MyCall(*this, iprm.callId);
        CallInfo ci = call->getInfo();
        CallOpParam prm;
        
        std::cout << "*** Incoming Call: " <<  ci.remoteUri << " ["
                  << ci.stateText << "]" << std::endl;
        
        calls.push_back(call);
        prm.statusCode = (pjsip_status_code)200;
        call->answer(prm);
    }
};

void MyCall::onCallState(OnCallStateParam &prm)
{
    PJ_UNUSED_ARG(prm);

    CallInfo ci = getInfo();
    std::cout << "*** Call: " <<  ci.remoteUri << " [" << ci.stateText
              << "]" << std::endl;
    
    if (ci.state == PJSIP_INV_STATE_DISCONNECTED) {
        myAcc->removeCall(this);
        /* Delete the call */
        delete this;
    }
}

void MyCall::onCallMediaState(OnCallMediaStateParam &prm)
{
    cout << "!!!!!      onCallMediaState is called     !!!!!" << endl;

    try{
        player.createPlayer("Ring02.wav", PJMEDIA_FILE_NO_LOOP);
        
        recorderVerify.createRecorder("test.wav");
        CallInfo ci = getInfo();
        AudioMedia* aud_med = 0;
        // Iterate all the call medias
        for (unsigned i = 0; i < ci.media.size(); i++) {
            cout << "Check audio " << i << endl;
            if (ci.media[i].type==PJMEDIA_TYPE_AUDIO && getMedia(i)) {
                aud_med = static_cast<AudioMedia*>( getMedia(i));
                break;
            }
        }
        if (aud_med != 0){
            cout << "Send stuff to media" << endl;

            // Connect the call audio media to sound device
            AudDevManager& mgr = Endpoint::instance().audDevManager();
            player.startTransmit(*aud_med);
            aud_med->startTransmit(recorder);
            player.startTransmit(recorderVerify);
        }

    } catch (Error& err) {
        cout << "Error when playing: " << err.info() << endl;
    }
}

static void mainProg(Endpoint &ep) throw(Error)
{
    // Init library
    EpConfig ep_cfg;
    ep_cfg.logConfig.level = 4;
    ep.libInit( ep_cfg );

    // Transport
    TransportConfig tcfg;
    tcfg.port = 5060;
    ep.transportCreate(PJSIP_TRANSPORT_UDP, tcfg);

    // Start library
    ep.libStart();
    std::cout << "*** PJSUA2 STARTED ***" << std::endl;

    // Add account
    AccountConfig acc_cfg;
    acc_cfg.idUri = "sip:ff@192.168.3.99";
    acc_cfg.regConfig.registrarUri = "sip:192.168.3.99";
    acc_cfg.sipConfig.authCreds.push_back( AuthCredInfo("digest", "asterisk",
                                                        "ff", 0, "ff") );
    MyAccount *acc(new MyAccount);
    acc->create(acc_cfg);
    
    pj_thread_sleep(2000);
    while(!acc->active);
    
    // Make outgoing call
    Call *call = new MyCall(*acc);
    acc->calls.push_back(call);
    CallOpParam prm(true);
    prm.opt.audioCount = 1;
    prm.opt.videoCount = 0;
    call->makeCall("sip:18900000000@192.168.3.99", prm);
    std::cout << "*** Start Calling ***" << std::endl;
    
    //Hangup all calls
    pj_thread_sleep(80000);
    ep.hangupAllCalls();
    pj_thread_sleep(4000);
    
    // Destroy library
    std::cout << "*** PJSUA2 SHUTTING DOWN ***" << std::endl;
    delete call;
    delete acc;
}


int main()
{
    int ret = 0;
    Endpoint ep;

    try {
	ep.libCreate();

	mainProg(ep);
	ret = PJ_SUCCESS;
    } catch (Error & err) {
	std::cout << "Exception: " << err.info() << std::endl;
	ret = 1;
    }

    try {
	ep.libDestroy();
    } catch(Error &err) {
	std::cout << "Exception: " << err.info() << std::endl;
	ret = 1;
    }

    if (ret == PJ_SUCCESS) {
	std::cout << "Success" << std::endl;
    } else {
	std::cout << "Error Found" << std::endl;
    }

    return ret;
}


