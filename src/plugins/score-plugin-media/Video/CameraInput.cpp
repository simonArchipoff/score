#include <Media/Libav.hpp>
#if SCORE_HAS_LIBAV
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}
#include "CameraInput.hpp"
#include <score/tools/Debug.hpp>

#include <QDebug>
#include <QElapsedTimer>


#include <functional>
namespace Video
{

CameraInput::CameraInput() noexcept
{
  realTime = true;
}

CameraInput::~CameraInput() noexcept
{
  close_file();
}

bool CameraInput::load(
    const std::string& inputKind,
    const std::string& inputDevice,
    int w,
    int h,
    double fps) noexcept
{
  close_file();
  m_inputKind = inputKind;
  m_inputDevice = inputDevice;

  auto ifmt = av_find_input_format(m_inputKind.c_str());
  if (ifmt)
  {
    qDebug() << ifmt->name << ifmt->long_name;
    return true;
  }
  else
  {
    return false;
  }
}

bool CameraInput::start() noexcept
{
  if (m_running)
    return false;

  auto ifmt = av_find_input_format(m_inputKind.c_str());
  if (!ifmt)
    return false;

  m_formatContext = avformat_alloc_context();
  m_formatContext->flags |= AVFMT_FLAG_NONBLOCK;
  m_formatContext->flags |= AVFMT_FLAG_NOBUFFER;

  /* TODO it seems that things work without that
  AVDictionary *options = nullptr;
  av_dict_set(&options, "framerate", std::to_string((int)fps).c_str(), 0);
  av_dict_set(&options, "input_format", format.c_str(), 0); // this one seems failing
  av_dict_set(&options, "video_size", fmt::format("{}x{}", w, h).c_str(), 0);
  */
  if (avformat_open_input(
          &m_formatContext, m_inputDevice.c_str(), ifmt, nullptr)
      != 0)
  {
    close_file();
    return false;
  }

  if (avformat_find_stream_info(m_formatContext, nullptr) < 0)
  {
    close_file();
    return false;
  }

  if (!open_stream())
  {
    close_file();
    return false;
  }

  m_running.store(true, std::memory_order_release);
  // TODO use a thread pool
  m_thread = std::thread{[this] { this->buffer_thread(); }};
  return true;
}

void CameraInput::stop() noexcept
{
  close_file();
}

AVFrame* CameraInput::dequeue_frame() noexcept
{
  return m_frames.dequeue();
}

void CameraInput::release_frame(AVFrame* frame) noexcept
{
  m_frames.release(frame);
}

void CameraInput::buffer_thread() noexcept
{
  while (m_running.load(std::memory_order_acquire))
  {
    if (auto f = read_frame_impl())
    {
      m_frames.enqueue(f);
    }
  }
}

void CameraInput::close_file() noexcept
{
  // Stop the running status
  m_running.store(false, std::memory_order_release);

  if (m_thread.joinable())
    m_thread.join();

  // Clear the stream
  close_stream();

  // Clear the fmt context
  if (m_formatContext)
  {
    avformat_close_input(&m_formatContext);
    m_formatContext = nullptr;
  }

  if (m_rescale)
  {
    sws_freeContext(m_rescale);
    m_rescale = nullptr;
  }

  // Remove frames that were in flight
  m_frames.drain();
}

AVFrame* CameraInput::read_frame_impl() noexcept
{
  ReadFrame res;

  if (m_stream != -1)
  {
    AVPacket packet;
    memset(&packet, 0, sizeof(AVPacket));

    do
    {
      res = read_one_frame(m_frames.newFrame(), packet);
    } while (res.error == AVERROR(EAGAIN));
  }
  return res.frame;
}

ReadFrame CameraInput::read_one_frame(AVFramePointer frame, AVPacket& packet)
{
  int res{};
  while ((res = av_read_frame(m_formatContext, &packet)) >= 0)
  {
    if (packet.stream_index == m_stream)
    {
      {
        SCORE_ASSERT(m_codecContext);
        auto res = enqueue_frame(&packet, std::move(frame));

        av_packet_unref(&packet);
        return res;
      }

      av_packet_unref(&packet);
      break;
    }

    av_packet_unref(&packet);
  }
  // if (res != 0 && res != AVERROR_EOF)
  //   qDebug() << "Error while reading a frame: "
  //            << av_make_error_string(
  //                   global_errbuf, sizeof(global_errbuf), res);
  av_packet_unref(&packet);
  return {nullptr, res};
}
void CameraInput::init_scaler() noexcept
{
  // Allocate a rescale context
  qDebug() << "allocating a rescaler for format"
           << av_get_pix_fmt_name(this->pixel_format);
  m_rescale = sws_getContext(
      this->width,
      this->height,
      this->pixel_format,
      this->width,
      this->height,
      AV_PIX_FMT_RGBA,
      SWS_FAST_BILINEAR,
      NULL,
      NULL,
      NULL);
  pixel_format = AV_PIX_FMT_RGBA;
}

bool CameraInput::open_stream() noexcept
{
  bool res = false;

  if (!m_formatContext)
  {
    close_stream();
    return false;
  }

  m_stream = -1;

  for (unsigned int i = 0; i < m_formatContext->nb_streams; i++)
  {
    auto codecpar = m_formatContext->streams[i]->codecpar;
    if (codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
    {
      m_stream = i;
      m_codec = avcodec_find_decoder(codecpar->codec_id);

      if (m_codec)
      {
        m_codecContext = avcodec_alloc_context3(m_codec);
        avcodec_parameters_to_context(m_codecContext, codecpar);

        m_codecContext->flags |= AV_CODEC_FLAG_LOW_DELAY;
        m_codecContext->flags2 |= AV_CODEC_FLAG2_FAST;

        res = !(avcodec_open2(m_codecContext, m_codec, nullptr) < 0);
        pixel_format = static_cast<AVPixelFormat>(codecpar->format);
        width = codecpar->width;
        height = codecpar->height;
        fps = av_q2d(m_formatContext->streams[i]->avg_frame_rate);

        switch (pixel_format)
        {
          // Supported formats for gpu decoding
          case AV_PIX_FMT_YUV420P:
          case AV_PIX_FMT_YUVJ420P:
          case AV_PIX_FMT_YUVJ422P:
          case AV_PIX_FMT_YUV422P:
          case AV_PIX_FMT_RGB0:
          case AV_PIX_FMT_RGBA:
          case AV_PIX_FMT_BGR0:
          case AV_PIX_FMT_BGRA:
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(56, 19, 100)
          case AV_PIX_FMT_GRAYF32LE:
          case AV_PIX_FMT_GRAYF32BE:
#endif
          case AV_PIX_FMT_GRAY8:
            break;
          // Other formats get rgb'd
          default:
          {
            init_scaler();
            break;
          }
        }
        break;
      }
    }
  }

  if (!res)
  {
    close_stream();
    return false;
  }

  return true;
}

void CameraInput::close_stream() noexcept
{
  if (m_codecContext)
  {
    avcodec_close(m_codecContext);
  }

  m_codecContext = nullptr;
  m_codec = nullptr;
  m_stream = -1;
}

ReadFrame CameraInput::enqueue_frame(const AVPacket* pkt, AVFramePointer frame) noexcept
{
  ReadFrame read = readVideoFrame(m_codecContext, pkt, frame.get());
  if(!read.frame)
  {
    this->m_frames.enqueue_decoding_error(frame.release());
    return read;
  }

  if (m_rescale)
  {
    // alloc an rgb frame
    auto rgb = m_frames.newFrame().release();
    av_frame_copy_props(rgb, read.frame);
    rgb->width = this->width;
    rgb->height = this->height;
    rgb->format = AV_PIX_FMT_RGBA;
    av_frame_get_buffer(rgb, 0);

    // 2. Resize
    sws_scale(
        m_rescale,
        read.frame->data,
        read.frame->linesize,
        0,
        this->height,
        rgb->data,
        rgb->linesize);

    // 3. Free the old frame data
    frame.reset();

    // 4. Return the new frame
    read.frame = rgb;
  }
  else
  {
    // it is already stored in "read" but well
    read.frame = frame.release();
  }
  return read;
}

}
#endif
