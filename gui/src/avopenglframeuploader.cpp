/*
 * This file is part of Chiaki.
 *
 * Chiaki is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Chiaki is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Chiaki.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <avopenglframeuploader.h>
#include <avopenglwidget.h>
#include <videodecoder.h>

#include <QOpenGLContext>
#include <QOpenGLFunctions>

#include <mainwindow.h>
#include <settings.h>
#include <QApplication>


AVOpenGLFrameUploader::AVOpenGLFrameUploader(VideoDecoder *decoder, AVOpenGLWidget *widget, QOpenGLContext *context, QSurface *surface)
	: QObject(nullptr),
	decoder(decoder),
	widget(widget),
	context(context),
	surface(surface)
{
	connect(decoder, SIGNAL(FramesAvailable()), this, SLOT(UpdateFrame()));
    
    //
	// Initialize ZMQ context and socket
	// TODO: Use POLL instead of PAIR
	//
    z_context = NULL;
    MainWindow *mainWnd = NULL;
    int idx = 0;
    while (idx < qApp->topLevelWidgets().length() && mainWnd == NULL)
    {
        mainWnd = dynamic_cast<MainWindow*>(qApp->topLevelWidgets()[idx]);
        idx++;
    }
    assert(mainWnd);
    
    if (mainWnd->getSettings()->GetDispatchServerState())
    {
        z_context = zmq_ctx_new();    
        z_socket = zmq_socket(z_context, ZMQ_PAIR);
        zmq_connect(z_socket, mainWnd->getSettings()->GetDispatchServerAddr().toStdString().c_str());
    }
        
}

AVOpenGLFrameUploader::~AVOpenGLFrameUploader()
{
    // Destory zmq
    if (z_context)
    {
        zmq_close(z_socket);
        zmq_ctx_destroy(z_context);
    }
}

void AVOpenGLFrameUploader::SendFrame(AVFrame *frame)
{
    //
    // FFMPEG AVFrame to OpenCV Mat conversion
    // based on https://timvanoosterhout.wordpress.com/2015/07/02/converting-an-ffmpeg-avframe-to-and-opencv-mat/
    //
    
    int width = frame->width;
    int height = frame->height;
 
    // Allocate the opencv mat and store its stride in a 1-element array
    cv::Mat image;
    if (image.rows != height || image.cols != width || image.type() != CV_8UC3) image = cv::Mat(height, width, CV_8UC3);
    int cvLinesizes[1];
    cvLinesizes[0] = image.step1();
 
    // Convert the colour format and write directly to the opencv matrix
    // Note here we use RGB24
    SwsContext* conversion = sws_getContext(width, height, (AVPixelFormat) frame->format, width, height, AV_PIX_FMT_RGB24, SWS_FAST_BILINEAR, NULL, NULL, NULL);
    sws_scale(conversion, frame->data, frame->linesize, 0, height, &image.data, cvLinesizes);
    sws_freeContext(conversion);
   
    
    //
    // Pack up image data with height, width and channels information
    // and send to Python server using ZMQ.
    //
    {
        assert(sizeof(unsigned short) == 2); // Make sure unsigned short length is 2
        int buf_len = sizeof(unsigned short) * 3 + image.total() * image.elemSize();
        unsigned char *buf = new unsigned char[buf_len];
        unsigned short height = image.size().height;
        unsigned short width = image.size().width;
        unsigned short channels = image.channels();
        memcpy(buf, &height, sizeof(unsigned short));
        memcpy(buf + sizeof(unsigned short), &width, sizeof(unsigned short));
        memcpy(buf + sizeof(unsigned short) * 2, &channels, sizeof(unsigned short));
        memcpy(buf + sizeof(unsigned short) * 3, image.data, image.total() * image.elemSize());

        zmq_msg_t msg;
        int rc = zmq_msg_init_size(&msg, buf_len);
        assert(rc==0);
        memcpy(zmq_msg_data(&msg), buf, buf_len);
        rc = zmq_msg_send(&msg, z_socket, 0);

        delete[] buf;
    }
}


void AVOpenGLFrameUploader::UpdateFrame()
{
	if(QOpenGLContext::currentContext() != context)
		context->makeCurrent(surface);

	AVFrame *next_frame = decoder->PullFrame();
	if(!next_frame)
		return;
    
    bool server_mode = false; // Chiaki server mode - no display in main window
    bool success = false;
    if (!server_mode)
        success = widget->GetBackgroundFrame()->Update(next_frame, decoder->GetChiakiLog());
        
    if (z_context)
        SendFrame(next_frame); // Send frame to ZMQ Server
	av_frame_free(&next_frame);

	if(success)
		widget->SwapFrames();
}
