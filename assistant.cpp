// Voice assistant example
//
// This program will detect your voice command and convert them to text.
//
// ref: https://github.com/ggerganov/whisper.cpp/issues/171
//

#include "common-sdl.h"
#include "common.h"
#include "whisper.h"
#include "grammar-parser.h"

#include <iostream>
#include <sstream>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <regex>
#include <string>
#include <thread>
#include <vector>
#include <map>
#include <string.h>
#include <netinet/in.h> 
#include <sys/socket.h> 
#include <sys/stat.h>
#include <unistd.h>

// command-line parameters
struct whisper_params {
    int32_t n_threads  = std::min(4, (int32_t) std::thread::hardware_concurrency());
    int32_t prompt_ms  = 5000;
    int32_t command_ms = 8000;
    int32_t capture_id = -1;
    int32_t max_tokens = 32;
    int32_t audio_ctx  = 0;

    float vad_thold  = 0.6f;
    float freq_thold = 100.0f;

    float grammar_penalty = 100.0f;

    grammar_parser::parse_state grammar_parsed;

    bool translate     = false;
    bool print_special = false;
    bool print_energy  = false;
    bool no_timestamps = true;
    bool use_gpu       = true;
    bool flash_attn    = false;

    std::string language  = "en";
    std::string model     = "models/ggml-base.en.bin";
    std::string fname_out;
    std::string commands;
    std::string prompt;
    std::string context;
    std::string grammar;

    // A regular expression that matches tokens to suppress
    std::string suppress_regex;
};

void whisper_print_usage(int argc, char ** argv, const whisper_params & params);

static bool whisper_params_parse(int argc, char ** argv, whisper_params & params) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            whisper_print_usage(argc, argv, params);
            exit(0);
        }
        else if (arg == "-t"   || arg == "--threads")       { params.n_threads     = std::stoi(argv[++i]); }
        else if (arg == "-pms" || arg == "--prompt-ms")     { params.prompt_ms     = std::stoi(argv[++i]); }
        else if (arg == "-cms" || arg == "--command-ms")    { params.command_ms    = std::stoi(argv[++i]); }
        else if (arg == "-c"   || arg == "--capture")       { params.capture_id    = std::stoi(argv[++i]); }
        else if (arg == "-mt"  || arg == "--max-tokens")    { params.max_tokens    = std::stoi(argv[++i]); }
        else if (arg == "-ac"  || arg == "--audio-ctx")     { params.audio_ctx     = std::stoi(argv[++i]); }
        else if (arg == "-vth" || arg == "--vad-thold")     { params.vad_thold     = std::stof(argv[++i]); }
        else if (arg == "-fth" || arg == "--freq-thold")    { params.freq_thold    = std::stof(argv[++i]); }
        else if (arg == "-tr"  || arg == "--translate")     { params.translate     = true; }
        else if (arg == "-ps"  || arg == "--print-special") { params.print_special = true; }
        else if (arg == "-pe"  || arg == "--print-energy")  { params.print_energy  = true; }
        else if (arg == "-ng"  || arg == "--no-gpu")        { params.use_gpu       = false; }
        else if (arg == "-fa"  || arg == "--flash-attn")    { params.flash_attn    = true; }
        else if (arg == "-l"   || arg == "--language")      { params.language      = argv[++i]; }
        else if (arg == "-m"   || arg == "--model")         { params.model         = argv[++i]; }
        else if (arg == "-f"   || arg == "--file")          { params.fname_out     = argv[++i]; }
        else if (arg == "-cmd" || arg == "--commands")      { params.commands      = argv[++i]; }
        else if (arg == "-p"   || arg == "--prompt")        { params.prompt        = argv[++i]; }
        else if (arg == "-ctx" || arg == "--context")       { params.context       = argv[++i]; }
        else if (                 arg == "--grammar")       { params.grammar       = argv[++i]; }
        else if (                 arg == "--grammar-penalty") { params.grammar_penalty = std::stof(argv[++i]); }
        else if (                 arg == "--suppress-regex") { params.suppress_regex = argv[++i]; }
        else {
            fprintf(stderr, "error: unknown argument: %s\n", arg.c_str());
            whisper_print_usage(argc, argv, params);
            exit(0);
        }
    }

    return true;
}

void whisper_print_usage(int /*argc*/, char ** argv, const whisper_params & params) {
    fprintf(stderr, "\n");
    fprintf(stderr, "usage: %s [options]\n", argv[0]);
    fprintf(stderr, "\n");
    fprintf(stderr, "options:\n");
    fprintf(stderr, "  -h,         --help           [default] show this help message and exit\n");
    fprintf(stderr, "  -t N,       --threads N      [%-7d] number of threads to use during computation\n", params.n_threads);
    fprintf(stderr, "  -pms N,     --prompt-ms N    [%-7d] prompt duration in milliseconds\n",             params.prompt_ms);
    fprintf(stderr, "  -cms N,     --command-ms N   [%-7d] command duration in milliseconds\n",            params.command_ms);
    fprintf(stderr, "  -c ID,      --capture ID     [%-7d] capture device ID\n",                           params.capture_id);
    fprintf(stderr, "  -mt N,      --max-tokens N   [%-7d] maximum number of tokens per audio chunk\n",    params.max_tokens);
    fprintf(stderr, "  -ac N,      --audio-ctx N    [%-7d] audio context size (0 - all)\n",                params.audio_ctx);
    fprintf(stderr, "  -vth N,     --vad-thold N    [%-7.2f] voice activity detection threshold\n",        params.vad_thold);
    fprintf(stderr, "  -fth N,     --freq-thold N   [%-7.2f] high-pass frequency cutoff\n",                params.freq_thold);
    fprintf(stderr, "  -tr,        --translate      [%-7s] translate from source language to english\n",   params.translate ? "true" : "false");
    fprintf(stderr, "  -ps,        --print-special  [%-7s] print special tokens\n",                        params.print_special ? "true" : "false");
    fprintf(stderr, "  -pe,        --print-energy   [%-7s] print sound energy (for debugging)\n",          params.print_energy ? "true" : "false");
    fprintf(stderr, "  -ng,        --no-gpu         [%-7s] disable GPU\n",                                 params.use_gpu ? "false" : "true");
    fprintf(stderr, "  -fa,        --flash-attn     [%-7s] flash attention\n",                             params.flash_attn ? "true" : "false");
    fprintf(stderr, "  -l LANG,    --language LANG  [%-7s] spoken language\n",                             params.language.c_str());
    fprintf(stderr, "  -m FNAME,   --model FNAME    [%-7s] model path\n",                                  params.model.c_str());
    fprintf(stderr, "  -f FNAME,   --file FNAME     [%-7s] text output file name\n",                       params.fname_out.c_str());
    fprintf(stderr, "  -cmd FNAME, --commands FNAME [%-7s] text file with allowed commands\n",             params.commands.c_str());
    fprintf(stderr, "  -p,         --prompt         [%-7s] the required activation prompt\n",              params.prompt.c_str());
    fprintf(stderr, "  -ctx,       --context        [%-7s] sample text to help the transcription\n",       params.context.c_str());
    fprintf(stderr, "  --grammar GRAMMAR            [%-7s] GBNF grammar to guide decoding\n",              params.grammar.c_str());
    fprintf(stderr, "  --grammar-penalty N          [%-7.1f] scales down logits of nongrammar tokens\n",   params.grammar_penalty);
    fprintf(stderr, "  --suppress-regex REGEX       [%-7s] regular expression matching tokens to suppress\n", params.suppress_regex.c_str());
    fprintf(stderr, "\n");
}

struct word {
    char *word; 
    bool status=false;
};

static bool voice_activation(word *&txt_word, size_t n, const char *prompt){
    printf("\nFinding keyword:");
    for (size_t i = 0; i < n; i++) {
        if (strcmp(prompt, (txt_word+i)->word)==0){
          (txt_word+i)->status = true;
          printf(" %s(found)", (txt_word+i)->word);
          return false;
        }
        printf(" %s,", (txt_word+i)->word);
    } 
    return true;
}

static std::string command(word *&txt_word, size_t n, std::string &txtmin){
  std::string argsh;
  struct stat sb;
  std::string dir = "dhika/assistant/";
  printf("\n\nWord count: %ld, Program directory: %s\n\nChecking:", n, dir.c_str());
  for (size_t i = 0; i < n; i++) {
    std::string name;
    if (!(txt_word+i)->status) {
      printf("\n\n(Checking)");
      for (size_t j = 0; j < n-i; j++) {
          name += (txt_word+i+j)->word;
          printf("\n%s", name.c_str());
          if (stat((dir+"scripts/"+name+".sh").c_str(), &sb) == 0){
            printf("\n\nExclude:");
            for (size_t k=i; k <= i+j; k++) {
              (txt_word+k)->status = true; 
              printf(" %s;", (txt_word+k)->word);
            }
            for (size_t k=i+j+1; k < n; k++) {
              argsh+= (txt_word+k)->word;
            }
            for (size_t i=0; i < n; i++) {
              if (!(txt_word+i)->status) {
                  argsh+=" ";
                  argsh+=(txt_word+i)->word;
              }
            }
            return dir+"scripts/"+name+".sh "+argsh;
          }
      }
      printf("\n(Done)");
    }
  }
  for (size_t i = 0; i < n; i++) {
      txtmin+= (txt_word+i)->word;
  }
  txtmin=dir+"aiscripts/"+txtmin+".sh";
  return "";
}

static std::string mintxt(std::string s, size_t &spaces)
{
    int j = 0;
    for (size_t i = 0; i < s.size(); i++) {
         
        // Store only valid characters
        if ((s[i] >= 'A' && s[i] <= 'Z') ||
            (s[i] >='a' && s[i] <= 'z') || 
            (s[i] >='1' && s[i] <= '9') ||
             s[i] == ' ')
        { 
            if (s[i] >= 'A' && s[i] <= 'Z') {
                s[i] = tolower(s[i]);
            }
            s[j] = s[i];
            j++;
            if (s[i] == ' ') {
               spaces++;
            } 
        }
    }
    return s.substr(0, j);
}

static std::string transcribe(
                 whisper_context * ctx,
            const whisper_params & params,
        const std::vector<float> & pcmf32,
               const std::string & grammar_rule,
                           float & logprob_min,
                           float & logprob_sum,
                             int & n_tokens,
                         int64_t & t_ms) {
    const auto t_start = std::chrono::high_resolution_clock::now();

    logprob_min = 0.0f;
    logprob_sum = 0.0f;
    n_tokens    = 0;
    t_ms = 0;

    //whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_BEAM_SEARCH);

    wparams.print_progress   = false;
    wparams.print_special    = params.print_special;
    wparams.print_realtime   = false;
    wparams.print_timestamps = !params.no_timestamps;
    wparams.translate        = params.translate;
    wparams.no_context       = true;
    wparams.no_timestamps    = params.no_timestamps;
    wparams.single_segment   = true;
    wparams.max_tokens       = params.max_tokens;
    wparams.language         = params.language.c_str();
    wparams.n_threads        = params.n_threads;

    wparams.audio_ctx = params.audio_ctx;

    wparams.temperature     = 0.4f;
    wparams.temperature_inc = 1.0f;
    wparams.greedy.best_of  = 5;

    wparams.beam_search.beam_size = 5;

    wparams.initial_prompt = params.context.data();

    wparams.suppress_regex = params.suppress_regex.c_str();

    const auto & grammar_parsed = params.grammar_parsed;
    auto grammar_rules = grammar_parsed.c_rules();

    if (!params.grammar_parsed.rules.empty() && !grammar_rule.empty()) {
        if (grammar_parsed.symbol_ids.find(grammar_rule) == grammar_parsed.symbol_ids.end()) {
            fprintf(stderr, "%s: warning: grammar rule '%s' not found - skipping grammar sampling\n", __func__, grammar_rule.c_str());
        } else {
            wparams.grammar_rules   = grammar_rules.data();
            wparams.n_grammar_rules = grammar_rules.size();
            wparams.i_start_rule    = grammar_parsed.symbol_ids.at(grammar_rule);
            wparams.grammar_penalty = params.grammar_penalty;
        }
    }

    if (whisper_full(ctx, wparams, pcmf32.data(), pcmf32.size()) != 0) {
        return "";
    }

    std::string result;

    const int n_segments = whisper_full_n_segments(ctx);
    for (int i = 0; i < n_segments; ++i) {
        const char * text = whisper_full_get_segment_text(ctx, i);

        result += text;

        const int n = whisper_full_n_tokens(ctx, i);
        for (int j = 0; j < n; ++j) {
            const auto token = whisper_full_get_token_data(ctx, i, j);

            if(token.plog > 0.0f) exit(0);
            logprob_min = std::min(logprob_min, token.plog);
            logprob_sum += token.plog;
            ++n_tokens;
        }
    }

    const auto t_end = std::chrono::high_resolution_clock::now();
    t_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count();

    return result;
}

static std::vector<std::string> get_words(const std::string &txt) {
    std::vector<std::string> words;

    std::istringstream iss(txt);
    std::string word;
    while (iss >> word) {
        words.push_back(word);
    }

    return words;
}

// general-purpose mode
// freely transcribe the voice into text
static int process_general_transcription(struct whisper_context * ctx, audio_async & audio, const whisper_params & params) {
    bool is_running  = true;
    bool have_prompt = false;
    bool ask_prompt  = true;

    float logprob_min0 = 0.0f;

    float logprob_sum0 = 0.0f;

    int n_tokens0 = 0;

    std::vector<float> pcmf32_cur;
    std::vector<float> pcmf32_prompt;

    int clientSocket;

    size_t word_n;

    char *copy, *action, *response; 

    char is_confirm;

    struct word* txt_word;

    std::string platform = "Fedora";

    std::string sqlstr, strword, txtmin, txtstrip, dir, msg;

    std::string k_prompt = "snowy";
    if (!params.prompt.empty()) {
        k_prompt = params.prompt;
    }

    fprintf(stderr, "\n");
    fprintf(stderr, "%s: general-purpose mode\n", __func__);

    // main loop
    while (is_running) {
        // handle Ctrl + C
        is_running = sdl_poll_events();

        // delay
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (ask_prompt) {
            fprintf(stdout, "\n");
            fprintf(stdout, "%s: Say the following phrase: '%s%s%s'\n", __func__, "\033[1m", k_prompt.c_str(), "\033[0m");
            fprintf(stdout, "\n");

            ask_prompt = false;
        }

        {
            audio.get(2000, pcmf32_cur);

            if (::vad_simple(pcmf32_cur, WHISPER_SAMPLE_RATE, 1000, params.vad_thold, params.freq_thold, params.print_energy)) {
                fprintf(stdout, "%s: Speech detected! Processing ...\n", __func__);

                int64_t t_ms = 0;

                if (!have_prompt) {
                    // wait for activation phrase
                    audio.get(params.prompt_ms, pcmf32_cur);

                    const auto txt = ::trim(::transcribe(ctx, params, pcmf32_cur, "prompt", logprob_min0, logprob_sum0, n_tokens0, t_ms));

                    const float p = 100.0f * std::exp(logprob_min0);

                    fprintf(stdout, "%s: Heard '%s%s%s', (t = %d ms, p = %.2f%%)\n", __func__, "\033[1m", txt.c_str(), "\033[0m", (int) t_ms, p);

                    word_n = 0;
    
                    txtstrip = mintxt(txt, word_n);
    
                    if(word_n)
                    {
                        word_n++;
    
                        copy = strdup(txtstrip.c_str());
    
                        // malloc to attach to memory
                        txt_word = reinterpret_cast<word*>(malloc(word_n*sizeof(struct word)));
    
                        (txt_word)->word = strtok(copy, " "); // Splits spaces between words in str 
                                                                                        
                        for(size_t i = 1; i < word_n; i++)
                        {
                            (txt_word+i)->word = strtok (NULL, " ");
                        }
        
                        if (voice_activation(txt_word, word_n, k_prompt.c_str())) {
                            fprintf(stdout, "\n%s: WARNING: prompt not recognized, try again\n", __func__);
                            ask_prompt = true;
                        } else {
                            fprintf(stdout, "\n");
                            fprintf(stdout, "%s: The prompt has been recognized!", __func__);
      
                            dir = command(txt_word, word_n, txtmin);
    
                            if (dir=="") {
                                  struct stat sb;
                                  if (stat(txtmin.c_str(), &sb) == 0){
                                        system(txtmin.c_str());
                                  }else {
                                        fprintf(stdout, "\n\n%s: Calculating!\n", __func__);
                                        // creating socket 
                                        clientSocket = socket(AF_INET, SOCK_STREAM, 0); 
                  
                                        // specifying address 
                                        sockaddr_in serverAddress; 
                                        serverAddress.sin_family = AF_INET; 
                                        serverAddress.sin_port = htons(8089); 
                                        serverAddress.sin_addr.s_addr = INADDR_ANY;
                    
                    
                                        msg = k_prompt+";"+platform+";"+txt;
                                        // sending connection request 
                                        connect(clientSocket, (struct sockaddr*)&serverAddress, 
                                              sizeof(serverAddress)); 
                                        send(clientSocket, msg.c_str(), strlen(msg.c_str()), 0); 
            
                                        char buffer[1024] = {0};
                                        recv(clientSocket, buffer, sizeof(buffer), 0);
                                        // closing socket 
                                        close(clientSocket);
    
                                        action = strtok(buffer, ";");
                                        response = strtok (NULL, ";");
                
                                        if (*action=='a') {
                                          printf("%s\n", response); 
                                        }else {
                                          printf("Writes:\n\n%s", response);
                                          printf("\n\nConfirm write?[Y/n]:");
                                          scanf(" %c",&is_confirm);
                                          if (is_confirm!='n') {
                                              std::ofstream fout(txtmin);
                                              fout<<response;
                                              fout.close();
                                              system(("sudo chmod a+x "+txtmin).c_str());
                                              system(txtmin.c_str());
                                          }
                                        }
                                    }
                            }else {
                                    fprintf(stdout, "\n\n%s: Running %s\n\n", __func__, dir.c_str());
                                    system(dir.c_str());
                            }
                        }
                        free(txt_word);
                        txtmin.clear();
                    }
                }
                audio.clear();
            }
        }
    }
    return 0;
}

int main(int argc, char ** argv) {
    whisper_params params;

    if (whisper_params_parse(argc, argv, params) == false) {
        return 1;
    }

    if (whisper_lang_id(params.language.c_str()) == -1) {
        fprintf(stderr, "error: unknown language '%s'\n", params.language.c_str());
        whisper_print_usage(argc, argv, params);
        exit(0);
    }

    // whisper init

    struct whisper_context_params cparams = whisper_context_default_params();

    cparams.use_gpu    = params.use_gpu;
    cparams.flash_attn = params.flash_attn;

    struct whisper_context * ctx = whisper_init_from_file_with_params(params.model.c_str(), cparams);

    // print some info about the processing
    {
        fprintf(stderr, "\n");
        if (!whisper_is_multilingual(ctx)) {
            if (params.language != "en" || params.translate) {
                params.language = "en";
                params.translate = false;
                fprintf(stderr, "%s: WARNING: model is not multilingual, ignoring language and translation options\n", __func__);
            }
        }
        fprintf(stderr, "%s: processing, %d threads, lang = %s, task = %s, timestamps = %d ...\n",
                __func__,
                params.n_threads,
                params.language.c_str(),
                params.translate ? "translate" : "transcribe",
                params.no_timestamps ? 0 : 1);

        fprintf(stderr, "\n");
    }

    // init audio

    audio_async audio(30*1000);
    if (!audio.init(params.capture_id, WHISPER_SAMPLE_RATE)) {
        fprintf(stderr, "%s: audio.init() failed!\n", __func__);
        return 1;
    }

    audio.resume();

    // wait for 1 second to avoid any buffered noise
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    audio.clear();

    int  ret_val = 0;

    if (!params.grammar.empty()) {
        auto & grammar = params.grammar_parsed;
        if (is_file_exist(params.grammar.c_str())) {
            // read grammar from file
            std::ifstream ifs(params.grammar.c_str());
            const std::string txt = std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
            grammar = grammar_parser::parse(txt.c_str());
        } else {
            // read grammar from string
            grammar = grammar_parser::parse(params.grammar.c_str());
        }

        // will be empty (default) if there are parse errors
        if (grammar.rules.empty()) {
            ret_val = 1;
        } else {
            fprintf(stderr, "%s: grammar:\n", __func__);
            grammar_parser::print_grammar(stderr, grammar);
            fprintf(stderr, "\n");
        }
    }

    if (ret_val == 0) {
        ret_val = process_general_transcription(ctx, audio, params);
    }

    audio.pause();

    whisper_print_timings(ctx);
    whisper_free(ctx);

    return ret_val;
}
