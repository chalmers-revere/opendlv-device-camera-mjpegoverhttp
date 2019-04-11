/*
 * Copyright (C) 2019  Christian Berger
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "cluon-complete.hpp"
#include "opendlv-standard-message-set.hpp"

#include <turbojpeg.h>
#include <curl/curl.h>
#include <X11/Xlib.h>
#include <libyuv.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>

uint32_t WIDTH{0};
uint32_t HEIGHT{0};
uint32_t ID{0};
bool VERBOSE{false};
bool SKIP_I420{false};
bool SKIP_ARGB{false};

std::unique_ptr<cluon::OD4Session> od4Session{nullptr};
std::unique_ptr<cluon::SharedMemory> sharedMemoryI420{nullptr};
std::unique_ptr<cluon::SharedMemory> sharedMemoryARGB{nullptr};

enum class ResponseState : uint8_t {
    MAIN_HEADER = 1,
    FRAME_HEADER = 2,
    FRAME = 3,
};
ResponseState g_state{ResponseState::MAIN_HEADER};

constexpr const uint32_t MAX_LINE_IN_HEADER{1000};
char lineFromResponseHeader[MAX_LINE_IN_HEADER] = { 0, };

constexpr const uint32_t JPEG_BUFFER_SIZE{8*1024*1024};
unsigned char compressedImageBuffer[JPEG_BUFFER_SIZE];

unsigned char *rawImageBGR24{nullptr};

uint32_t numberOfJPEGHeaders{0};
uint32_t compressedImageBufferIndex{0};
uint32_t expectedSizeOfJPEG{0};

tjhandle g_jpegDecompressor{nullptr};

Display* g_display{nullptr};
Visual* g_visual{nullptr};
Window g_window{0};
XImage* g_ximage{nullptr};

static std::mutex recFileMutex;
static std::shared_ptr<std::fstream> recFile{nullptr};

void decodeCompressedJPEGFrame(unsigned char *ptr, int len) {
    cluon::data::TimeStamp ts{cluon::time::now()};

    if ( (nullptr != rawImageBGR24) && (nullptr != ptr) && (0 < len) ) {
        int width{0};
        int height{0};
        int subsampling{0};
        tjDecompressHeader2(g_jpegDecompressor, ptr, len, &width, &height, &subsampling);

        if ( ((0 < width) && (static_cast<uint32_t>(width) == WIDTH)) && 
             ((0 < height) && (static_cast<uint32_t>(height) == HEIGHT)) ) {

            // Dump JPEG directly to .rec file.
            {
                std::lock_guard<std::mutex> lck(recFileMutex);
                if (recFile && recFile->good() && (nullptr != ptr) && (0 < len) ) {
                    std::string data{reinterpret_cast<char*>(ptr), static_cast<size_t>(len)};
                    if (!data.empty()) {
                        opendlv::proxy::ImageReading ir;
                        ir.fourcc("jfif")
                          .width(WIDTH)
                          .height(HEIGHT)
                          .data(data);
                        cluon::data::Envelope envelope;
                        {
                            cluon::ToProtoVisitor protoEncoder;
                            {
                                envelope.dataType(ir.ID());
                                ir.accept(protoEncoder);
                                envelope.serializedData(protoEncoder.encodedData());
                                envelope.sent(cluon::time::now());
                                envelope.sampleTimeStamp(ts);
                                envelope.senderStamp(ID);
                            }
                        }

                        std::string serializedData{cluon::serializeEnvelope(std::move(envelope))};
                        recFile->write(serializedData.data(), serializedData.size());
                        recFile->flush();
                    }
                }
            }

            // Decompress JPEG.
            tjDecompress2(g_jpegDecompressor, ptr, len, rawImageBGR24, width, 0 /*pitch*/, height, TJPF_RGB, TJFLAG_FASTDCT);

            sharedMemoryI420->lock();
            sharedMemoryI420->setTimeStamp(ts);
            if (!SKIP_I420 || !SKIP_ARGB /* ARGB conversion needs I420. */) {
                libyuv::RAWToI420(reinterpret_cast<uint8_t*>(rawImageBGR24), WIDTH * 3 /* 3*WIDTH for RGB24*/,
                                    reinterpret_cast<uint8_t*>(sharedMemoryI420->data()), WIDTH,
                                    reinterpret_cast<uint8_t*>(sharedMemoryI420->data()+(WIDTH * HEIGHT)), WIDTH/2,
                                    reinterpret_cast<uint8_t*>(sharedMemoryI420->data()+(WIDTH * HEIGHT + ((WIDTH * HEIGHT) >> 2))), WIDTH/2,
                                    WIDTH, HEIGHT);
            }
            sharedMemoryI420->unlock();

            sharedMemoryARGB->lock();
            sharedMemoryARGB->setTimeStamp(ts);
            if (!SKIP_ARGB) {
                libyuv::I420ToARGB(reinterpret_cast<uint8_t*>(sharedMemoryI420->data()), WIDTH,
                                   reinterpret_cast<uint8_t*>(sharedMemoryI420->data()+(WIDTH * HEIGHT)), WIDTH/2,
                                   reinterpret_cast<uint8_t*>(sharedMemoryI420->data()+(WIDTH * HEIGHT + ((WIDTH * HEIGHT) >> 2))), WIDTH/2,
                                   reinterpret_cast<uint8_t*>(sharedMemoryARGB->data()), WIDTH * 4, WIDTH, HEIGHT);

                if (VERBOSE) {
                    XPutImage(g_display, g_window, DefaultGC(g_display, 0), g_ximage, 0, 0, 0, 0, WIDTH, HEIGHT);
                    std::clog << "[opendlv-device-camera-mjpegoverhttp]: Acquired new frame at " << cluon::time::toMicroseconds(ts) << " microseconds." << std::endl;
                }
            }
            sharedMemoryARGB->unlock();

            sharedMemoryI420->notifyAll();
            sharedMemoryARGB->notifyAll();
        }
        else {
            std::cerr << "[opendlv-device-camera-mjpegoverhttp]: Error: expecting width " << WIDTH << ", found " << width << ", expecting height " << HEIGHT << ", found " << height << std::endl;
        }
    }
}

size_t receiveData(void *ptr, size_t size, size_t nmemb, FILE*) {
    unsigned char *ucptr = static_cast<unsigned char*>(ptr);
    size_t nbytes = size * nmemb;
    for (size_t i{0}; i < nbytes; i++) {
        unsigned char b = ucptr[i];
        if ( (g_state == ResponseState::MAIN_HEADER) ||
             (g_state == ResponseState::FRAME_HEADER) ) {
            size_t len = strlen(lineFromResponseHeader);
            if (len < MAX_LINE_IN_HEADER) {
                lineFromResponseHeader[len] = b;
            }

            // Try finding line break and remove it from buffer.
            if ('\n' == b) {
                if (lineFromResponseHeader[len-1] == '\r') {
                    lineFromResponseHeader[len] = 0;
                    lineFromResponseHeader[len-1] = 0;
                    // Decode content-length.
                    if (strncasecmp(lineFromResponseHeader, "content-length:", 15) == 0) {
                        expectedSizeOfJPEG = atoi(lineFromResponseHeader + 16);
//                        std::clog << "[opendlv-device-camera-mjpegoverhttp]: Found content-length" << expectedSizeOfJPEG << std::endl;
                    }
                    // Decode content-type.
//                    if (strncasecmp(lineFromResponseHeader, "content-type:", 13) == 0) {
//                        std::string content{lineFromResponseHeader+14};
//                        std::clog << "[opendlv-device-camera-mjpegoverhttp]: Found content-type" << content << std::endl;
//                    }
                    // Decode boundary information.
                    if (strncmp(lineFromResponseHeader, "--", 2) == 0) {
                        g_state = ResponseState::FRAME_HEADER;
                    }
                    // Search for frame data.
                    if (strlen(lineFromResponseHeader) == 0 && numberOfJPEGHeaders > 0) {
                        // Wait for an empty line to get frame.
                        if (g_state == ResponseState::FRAME_HEADER) {
                            g_state = ResponseState::FRAME;
                            compressedImageBufferIndex = 0;
                        }
                    }
                    // Clear buffer.
                    std::memset(&lineFromResponseHeader, 0, MAX_LINE_IN_HEADER);
                    numberOfJPEGHeaders++;
                }
            }
        }
        else if (g_state == ResponseState::FRAME) {
            if (compressedImageBufferIndex < JPEG_BUFFER_SIZE) {
                compressedImageBuffer[compressedImageBufferIndex] = b;
            }
            compressedImageBufferIndex++;
            if (compressedImageBufferIndex >= expectedSizeOfJPEG) {
                decodeCompressedJPEGFrame(compressedImageBuffer, expectedSizeOfJPEG);

                // Clear buffer and reset state machine.
                g_state = ResponseState::FRAME_HEADER;
                numberOfJPEGHeaders = 0;
                compressedImageBufferIndex = 0;
                expectedSizeOfJPEG = 0;
                memset(lineFromResponseHeader, 0, 1000);
            }
        }
    }
    return nbytes;
}

int32_t main(int32_t argc, char **argv) {
    int32_t retCode{1};
    auto commandlineArguments = cluon::getCommandlineArguments(argc, argv);
    if ( (0 == commandlineArguments.count("url")) ||
         (0 == commandlineArguments.count("width")) ||
         (0 == commandlineArguments.count("height")) ) {
        std::cerr << argv[0] << " interfaces with the given OpenCV-encapsulated camera (e.g., a V4L identifier like 0 or a stream address) and provides the captured image in two shared memory areas: one in I420 format and one in ARGB format." << std::endl;
        std::cerr << "Usage:   " << argv[0] << " --url=<URL> --width=<width> --height=<height> [--name.i420=<unique name for the shared memory in I420 format>] [--name.argb=<unique name for the shared memory in ARGB format>] [--cid=<CID> [--id<ID>] --remote] [--rec=<Name of recording file>] [--recsuffix<Suffix to append] [--verbose]" << std::endl;
        std::cerr << "         --cid:       CID of the OD4Session to receive Envelopes for recording" << std::endl;
        std::cerr << "         --id:        ID to use in case of multiple instances of" << argv[0] << std::endl;
        std::cerr << "         --rec:       name of the recording file; default: YYYY-MM-DD_HHMMSS.rec" << std::endl;
        std::cerr << "         --recsuffix: additional suffix to add to the .rec file" << std::endl;
        std::cerr << "         --remote:    listen for cluon.data.RecorderCommand to start/stop recording" << std::endl;
        std::cerr << "         --url:       URL providing an MJPEG stream over http" << std::endl;
        std::cerr << "         --name.i420: name of the shared memory for the I420 formatted image; when omitted, video0.i420 is chosen" << std::endl;
        std::cerr << "         --name.argb: name of the shared memory for the I420 formatted image; when omitted, video0.argb is chosen" << std::endl;
        std::cerr << "         --width:     desired width of a frame" << std::endl;
        std::cerr << "         --height:    desired height of a frame" << std::endl;
        std::cerr << "         --freq:      desired frame rate" << std::endl;
        std::cerr << "         --skip.i420: don't decode MJPEG frame into i420 format; default: false" << std::endl;
        std::cerr << "         --skip.argb: don't decode MJPEG frame into argb format; default: false" << std::endl;
        std::cerr << "         --verbose:   display captured image" << std::endl;
        std::cerr << "Example: " << argv[0] << " --url=http://192.168.0.11?mjpeg --width=640 --height=480 --verbose --rec=myFile.rec --cid=111 --remote" << std::endl;
    } else {
        const std::string URL{commandlineArguments["url"]};
        const std::string NAME_I420{(commandlineArguments["name.i420"].size() != 0) ? commandlineArguments["name.i420"] : "video0.i420"};
        const std::string NAME_ARGB{(commandlineArguments["name.argb"].size() != 0) ? commandlineArguments["name.argb"] : "video0.argb"};
        ID = ( (commandlineArguments.count("id") != 0) ? static_cast<uint32_t>(std::stoi(commandlineArguments["id"])) : 0 );
        WIDTH = static_cast<uint32_t>(std::stoi(commandlineArguments["width"]));
        HEIGHT = static_cast<uint32_t>(std::stoi(commandlineArguments["height"]));
        VERBOSE = (commandlineArguments.count("verbose") != 0);
        SKIP_I420 = (commandlineArguments.count("skip.i420") != 0);
        SKIP_ARGB = (commandlineArguments.count("skip.argb") != 0);

        sharedMemoryI420.reset(new cluon::SharedMemory{NAME_I420, WIDTH * HEIGHT * 3/2});
        if (!sharedMemoryI420 || !sharedMemoryI420->valid()) {
            std::cerr << "[opendlv-device-camera-mjpegoverhttp]: Failed to create shared memory '" << NAME_I420 << "'." << std::endl;
            return retCode;
        }

        sharedMemoryARGB.reset(new cluon::SharedMemory{NAME_ARGB, WIDTH * HEIGHT * 4});
        if (!sharedMemoryARGB || !sharedMemoryARGB->valid()) {
            std::cerr << "[opendlv-device-camera-mjpegoverhttp]: Failed to create shared memory '" << NAME_ARGB << "'." << std::endl;
            return retCode;
        }

        if ( (sharedMemoryI420 && sharedMemoryI420->valid()) &&
             (sharedMemoryARGB && sharedMemoryARGB->valid()) ) {
            std::clog << "[opendlv-device-camera-mjpegoverhttp]: Data from camera '" << URL<< "' available in I420 format in shared memory '" << sharedMemoryI420->name() << "' (" << sharedMemoryI420->size() << ") and in ARGB format in shared memory '" << sharedMemoryARGB->name() << "' (" << sharedMemoryARGB->size() << ")." << std::endl;

            // Accessing the low-level X11 data display.
            if (VERBOSE) {
                g_display = XOpenDisplay(NULL);
                g_visual = DefaultVisual(g_display, 0);
                g_window = XCreateSimpleWindow(g_display, RootWindow(g_display, 0), 0, 0, WIDTH, HEIGHT, 1, 0, 0);
                sharedMemoryARGB->lock();
                {
                    g_ximage = XCreateImage(g_display, g_visual, 24, ZPixmap, 0, sharedMemoryARGB->data(), WIDTH, HEIGHT, 32, 0);
                }
                sharedMemoryARGB->unlock();
                XMapWindow(g_display, g_window);
            }

            {
                auto getYYYYMMDD_HHMMSS = [](){
                    cluon::data::TimeStamp now = cluon::time::now();

                    const long int _seconds = now.seconds();
                    struct tm *tm = localtime(&_seconds);

                    uint32_t year = (1900 + tm->tm_year);
                    uint32_t month = (1 + tm->tm_mon);
                    uint32_t dayOfMonth = tm->tm_mday;
                    uint32_t hours = tm->tm_hour;
                    uint32_t minutes = tm->tm_min;
                    uint32_t seconds = tm->tm_sec;

                    std::stringstream sstr;
                    sstr << year << "-" << ( (month < 10) ? "0" : "" ) << month << "-" << ( (dayOfMonth < 10) ? "0" : "" ) << dayOfMonth
                                   << "_" << ( (hours < 10) ? "0" : "" ) << hours
                                   << ( (minutes < 10) ? "0" : "" ) << minutes
                                   << ( (seconds < 10) ? "0" : "" ) << seconds;

                    std::string retVal{sstr.str()};
                    return retVal;
                };

                const bool REMOTE{commandlineArguments.count("remote") != 0};
                const std::string RECSUFFIX{commandlineArguments["recsuffix"]};
                const std::string REC{(commandlineArguments["rec"].size() != 0) ? commandlineArguments["rec"] : ""};
                const std::string NAME_RECFILE{(commandlineArguments["rec"].size() != 0) ? commandlineArguments["rec"] + RECSUFFIX : (getYYYYMMDD_HHMMSS() + RECSUFFIX + ".rec")};

                if (!REMOTE) {
                    recFile.reset(new std::fstream(NAME_RECFILE.c_str(), std::ios::out|std::ios::binary|std::ios::trunc));
                    std::clog << argv[0] << ": Created " << NAME_RECFILE << "." << std::endl;
                }
                else {
                    if (commandlineArguments.count("cid") == 0) {
                        std::cerr << "[opendlv-device-camera-mjpegoverhttp]: --remote specified but no --cid=? provided." << std::endl;
                        return retCode;
                    }

                    std::string nameOfRecFile;
                    od4Session.reset(new cluon::OD4Session(static_cast<uint16_t>(std::stoi(commandlineArguments["cid"])),
                        [argv, REC, RECSUFFIX, getYYYYMMDD_HHMMSS, &nameOfRecFile](cluon::data::Envelope &&envelope) noexcept {
                        if (cluon::data::RecorderCommand::ID() == envelope.dataType()) {
                            std::lock_guard<std::mutex> lck(recFileMutex);
                            cluon::data::RecorderCommand rc = cluon::extractMessage<cluon::data::RecorderCommand>(std::move(envelope));
                            if (1 == rc.command()) {
                                if (recFile && recFile->good()) {
                                    recFile->flush();
                                    recFile->close();
                                    recFile = nullptr;
                                    std::clog << argv[0] << ": Closed " << nameOfRecFile << "." << std::endl;
                                }
                                nameOfRecFile = (REC.size() != 0) ? REC + RECSUFFIX : (getYYYYMMDD_HHMMSS() + RECSUFFIX + ".rec");
                                recFile.reset(new std::fstream(nameOfRecFile.c_str(), std::ios::out|std::ios::binary|std::ios::trunc));
                                std::clog << argv[0] << ": Created " << nameOfRecFile << "." << std::endl;
                            }
                            else if (2 == rc.command()) {
                                if (recFile && recFile->good()) {
                                    recFile->flush();
                                    recFile->close();
                                    std::clog << argv[0] << ": Closed " << nameOfRecFile << "." << std::endl;
                                }
                                recFile = nullptr;
                            }
                        }
                    }));
                }
            }

            rawImageBGR24 = new unsigned char[WIDTH * HEIGHT * 3];
            g_jpegDecompressor = tjInitDecompress();

            CURL *curl = curl_easy_init();
            curl_easy_setopt(curl, CURLOPT_URL, URL.c_str());
            curl_easy_setopt(curl, CURLOPT_VERBOSE, (VERBOSE ? 1 : 0));
            curl_easy_setopt(curl, CURLOPT_HEADER, 0);
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
            curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 0);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 5000);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, receiveData);
            curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 60L);
            curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);

            curl_easy_perform(curl);

            if (VERBOSE) {
                XCloseDisplay(g_display);
            }
            curl_easy_cleanup(curl);

            tjDestroy(g_jpegDecompressor);

            delete [] rawImageBGR24;
        }
        retCode = 0;
    }
    return retCode;
}

