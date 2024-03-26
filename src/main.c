#include <pocketsphinx.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <portaudio.h>
#include <unistd.h>

static int globalDone = 0;
static void CatchSig(int signum)
{
    (void)signum;
    globalDone = 1;
}


#define MODELDIR "/Users/dmitriiborejko/Desktop/code/ML/SpeechRecognition_C/model"

// const char* keyfile = "алиса/3.16227766016838e-13/\n";

static ps_config_t* createConfig(){
    ps_config_t *config = ps_config_init(NULL);
    ps_config_set_str(config, "hmm", MODELDIR "/zero_ru.cd_cont_4000/");

    ps_config_set_str(config, "dict", MODELDIR "/myDictColor.dic");

    return config;
}

static void createKWS(ps_decoder_t* decoder){
    if(ps_add_kws(decoder, "keyword_search", MODELDIR "/kws") != 0){
        E_FATAL("kws add faild");
        return;
    }

    if(ps_activate_search(decoder, "keyword_search") != 0){
        E_FATAL("kws active faild");
        return;
    }

    ps_add_jsgf_file(decoder, "jsgf_model", MODELDIR "/color.jsgf");
    // ps_add_lm_file(decoder, "lm",  MODELDIR "/color.lm.bin");
    // ngram_model_t *lm = ps_get_lm(decoder, "lm");
    // ps_add_lm(decoder, "lm_model",  lm);
}

int main(int argc, char * argv[]){
    PaStream *stream;
    PaError err;
    ps_decoder_t *decoder;
    ps_config_t *config;
    ps_endpointer_t *ep;
    short *frame;
    size_t frameSize;

    (void)argc;
    (void)argv;

    config = createConfig();

    if ((err = Pa_Initialize()) != paNoError)
        E_FATAL("Failed to initialize PortAudio: %s\n",
                Pa_GetErrorText(err));
    
    if ((decoder = ps_init(config)) == NULL)
        E_FATAL("PocketSphinx decoder init failed\n");
    
    if ((ep = ps_endpointer_init(0, 0.0, 0, 0, 0)) == NULL)
        E_FATAL("PocketSphinx endpointer init failed\n");

    frameSize = ps_endpointer_frame_size(ep);

    
    if ((frame = malloc(frameSize * sizeof(frame[0]))) == NULL)
        E_FATAL_SYSTEM("Failed to allocate frame");
    
    if ((err = Pa_OpenDefaultStream(&stream, 1, 0, paInt16,
                                    ps_config_int(config, "samprate"),
                                    frameSize, NULL, NULL)) != paNoError)
        E_FATAL("Failed to open PortAudio stream: %s\n",
                Pa_GetErrorText(err));
    
    if ((err = Pa_StartStream(stream)) != paNoError)
        E_FATAL("Failed to start PortAudio stream: %s\n",
                Pa_GetErrorText(err));
    
    if (signal(SIGINT, CatchSig) == SIG_ERR)
        E_FATAL_SYSTEM("Failed to set SIGINT handler");

    createKWS(decoder);
    // printf("done");
    uint8_t kws = 0;
    uint8_t canselListeningFlags = 1;
    while(!globalDone){
        const int16 *speech;
        int prevInSpeech = ps_endpointer_in_speech(ep);
        if((err = Pa_ReadStream(stream, frame, frameSize)) != paNoError){
            E_ERROR("Error in PortAuddio read: %s\n", Pa_GetErrorText(err));
            break;
        }
        char buff[50];
        speech = ps_endpointer_process(ep, frame);
        if (speech != NULL) {
            const char *hyp;
            
            if (!prevInSpeech && canselListeningFlags) {
                // fprintf(stderr, "Speech start at %.2f\n",
                ps_endpointer_speech_start(ep);
		        fflush(stderr);
                canselListeningFlags = 0;
                ps_start_utt(decoder);
            }
            if (ps_process_raw(decoder, speech, frameSize, FALSE, FALSE) < 0)
                E_FATAL("ps_process_raw() failed\n");
            
            if((hyp = ps_get_hyp(decoder, NULL)) != NULL){
                //swich mode
                if (hyp != NULL && !kws && (strncmp(hyp, "алиса", 5) == 0)){
                    ps_end_utt(decoder);
                    if(ps_activate_search(decoder, "jsgf_model") != 0)
                        E_FATAL("ERROR: Cannot switch.\n");
                    kws = 1;
                    printf("LISTENING...  (switch to jsgf_model)\n");
                    ps_start_utt(decoder);
                    
                } 

                if(kws && hyp != NULL && (strncmp(hyp, "спасибо", 7) == 0)){
                    ps_end_utt(decoder);
                    if(ps_activate_search(decoder, "keyword_search") != 0)
                        E_FATAL("ERROR: Cannot switch.\n");
                    kws = 0;
                    printf("(switch to listening model)\n");
                    ps_start_utt(decoder);
                 }
            }
            if(!ps_endpointer_in_speech(ep)){
                        if(hyp != NULL){
                            fprintf(stderr, "PARTIAL RESULT: %s\n", hyp);
                            fflush(stderr);
                        }
                        ps_end_utt(decoder);
                        canselListeningFlags = 1;
                }

        }

    }
    
    if ((err = Pa_StopStream(stream)) != paNoError)
        E_FATAL("Failed to stop PortAudio stream: %s\n",
                Pa_GetErrorText(err));
    if ((err = Pa_Terminate()) != paNoError)
        E_FATAL("Failed to terminate PortAudio: %s\n",
                Pa_GetErrorText(err));
    free(frame);
    ps_endpointer_free(ep);
    ps_free(decoder);
    ps_config_free(config);

    return 0;
}
