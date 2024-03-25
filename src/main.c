#include <pocketsphinx.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <portaudio.h>

static int globalDone = 0;
static void CatchSig(int signum)
{
    (void)signum;
    globalDone = 1;
}


#define MODELDIR "/Users/dmitriiborejko/Desktop/code/ML/SpeechRecognition_C/model"

static ps_config_t* createConfig(){
    ps_config_t *config = ps_config_init(NULL);
    ps_config_set_str(config, "hmm", MODELDIR "/zero_ru.cd_cont_4000/");
    ps_config_set_str(config, "jsgf", MODELDIR "/color.jsgf");
    ps_config_set_str(config, "dict", MODELDIR "/myDictColor.dic");
    return config;
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

    
    // printf("done");
    
    while(!globalDone){
        const int16 *speech;
        int prevInSpeech = ps_endpointer_in_speech(ep);
        if((err = Pa_ReadStream(stream, frame, frameSize)) != paNoError){
            E_ERROR("Error in PortAuddio read: %s\n", Pa_GetErrorText(err));
            break;
        }

        speech = ps_endpointer_process(ep, frame);
        if (speech != NULL) {
            const char *hyp;
            if (!prevInSpeech) {
                fprintf(stderr, "Speech start at %.2f\n",
                        ps_endpointer_speech_start(ep));
		        fflush(stderr);
                ps_start_utt(decoder);
            }
            if (ps_process_raw(decoder, speech, frameSize, FALSE, FALSE) < 0)
                E_FATAL("ps_process_raw() failed\n");
            
            if (ps_get_hyp(decoder, NULL) != NULL){
                hyp = ps_get_hyp(decoder, NULL);
                if(strncmp(hyp, "алиса", 5) != 0){
                    continue;
                }
                else if(strncmp(hyp, "алиса", 5) == 0){
                    hyp = ps_get_hyp(decoder, NULL);
                }
                // printf("res: %s\n", hyp);
                fflush(stderr);
            }
            if (!ps_endpointer_in_speech(ep)) {
                fprintf(stderr, "Speech end at %.2f\n", ps_endpointer_speech_end(ep));
                    fflush(stderr);
                    fprintf(stderr, "PARTIAL RESULT: %s\n", hyp);
		            fflush(stderr);
                    ps_end_utt(decoder);
                    if ((hyp = ps_get_hyp(decoder, NULL)) != NULL) {
                        printf("%s\n", hyp);
                        fflush(stdout);
		        }
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
