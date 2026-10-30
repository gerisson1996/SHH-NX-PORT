/**
 * @file        audio/switch/switch_audio_system.cpp
 * @brief       Audio output for Nintendo Switch through libnx audout
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include <rex/audio/switch/switch_audio_system.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include <rex/assert.h>
#include <rex/audio/conversion.h>
#include <rex/audio/flags.h>
#include <rex/cvar.h>
#include <rex/logging.h>

#include <switch.h>

// Passive counters: the audio thread neither writes reports nor pauses the game.
extern "C" void RexSwitchPerfAdd(unsigned id, u64 value);
extern "C" void RexSwitchPerfMax(unsigned id, u64 value);

REXCVAR_DEFINE_BOOL(audio_mute, false, "Audio", "Mute audio output");
REXCVAR_DEFINE_BOOL(audio_switch_pump, true, "Audio",
                    "Ask the game for one frame every 5.333 ms (187.5 Hz pump, like "
                    "MarathonRecomp-NX) instead of 4 in a row when each audout buffer is consumed, which "
                    "left the game's audio server voice without a packet (robotic audio); "
                    "read when the audio is opened");
REXCVAR_DEFINE_INT32(audio_switch_frames_in_queue, 10, "Audio",
                     "With audio_switch_pump: game frames kept queued before audout "
                     "(cushion; 10 = 53 ms, 6 up to build 105). A longer queue lets the game wait without cutting the "
                     "output, at the cost of latency: sa2_audio_wait_server_ms waits up to 30 ms; read "
                     "when the audio is opened");
REXCVAR_DEFINE_INT32(audio_switch_buffer_frames, 4, "Audio",
                     "Frames per audout buffer (1-4, default 4 = 1024 samples = 21.3 ms). "
                     "Lower values reduce output latency at the cost of more frequent buffer completions; "
                     "read when the audio is opened");
REXCVAR_DEFINE_INT32(audio_switch_buffers, 3, "Audio",
                     "Number of audout buffers kept in rotation (2-4, default 3). "
                     "With 2 buffers, latency is lower than 3 buffers; "
                     "read when the audio is opened");

namespace rex::audio::nx {

namespace {

// Maximum guest frames per audout buffer, and buffers kept queued in the device.
// Arrays are sized for the maximum (4 frames = 1024 samples = 21.3 ms, 4 buffers).
// Runtime values are governed by audio_switch_buffer_frames and audio_switch_buffers.
constexpr uint32_t kMaxFramesPerBuffer = 4;
constexpr uint32_t kMaxBufferCount = 4;
constexpr uint32_t kMaxBufferSamples = kMaxFramesPerBuffer * SwitchAudioDriver::kChannelSamples;
constexpr uint32_t kMaxBufferBytes = kMaxBufferSamples * 2 * sizeof(s16);
// audout wants page-aligned sample memory.
constexpr uint32_t kBufferAllocBytes = (kMaxBufferBytes + 0xFFF) & ~0xFFFu;

// Above guest threads (0x3B) so the device is never starved, like the audio
// pump of MarathonRecomp-NX (0x2B).
constexpr int kOutputThreadPriority = 0x2B;
constexpr size_t kOutputThreadStack = 0x10000;
// Bounded so the thread notices shutdown even if the device stops releasing.
constexpr u64 kWaitTimeoutNs = 100'000'000;
// audio_switch_pump: one frame (256 samples) every 5.333 ms with absolute deadlines. With this many
// frames queued or fewer, another one is requested on each tick, and below the minimum one more is
// requested to catch up (the system clock and the audio clock do not match exactly).
constexpr u64 kPumpIntervalNs = 1'000'000'000ull * SwitchAudioDriver::kChannelSamples / 48000;
constexpr size_t kTargetQueuedFrames = 6;
constexpr size_t kMinQueuedFrames = 2;

}  // namespace

/* --- driver ------------------------------------------------------------------ */

SwitchAudioDriver::SwitchAudioDriver(memory::Memory* memory, rex::thread::Semaphore* semaphore)
    : AudioDriver(memory), semaphore_(semaphore) {}

SwitchAudioDriver::~SwitchAudioDriver() {
  std::lock_guard<std::mutex> guard(frames_mutex_);
  while (!frames_unused_.empty()) {
    delete[] frames_unused_.top();
    frames_unused_.pop();
  }
  while (!frames_queued_.empty()) {
    delete[] frames_queued_.front();
    frames_queued_.pop();
  }
}

void SwitchAudioDriver::SubmitFrame(uint32_t frame_ptr) {
  // Runs inside the guest callback while it holds the global critical region:
  // copy and return, never block on the device.
  const auto input_frame = memory_->TranslateVirtual<float*>(frame_ptr);
  float* output_frame;
  {
    std::lock_guard<std::mutex> guard(frames_mutex_);
    if (frames_unused_.empty()) {
      output_frame = new float[kFrameSamples];
    } else {
      output_frame = frames_unused_.top();
      frames_unused_.pop();
    }
  }
  std::memcpy(output_frame, input_frame, kFrameSamples * sizeof(float));
  std::lock_guard<std::mutex> guard(frames_mutex_);
  frames_queued_.push(output_frame);
}

bool SwitchAudioDriver::MixFrameInto(float* stereo_out, const StereoFold& fold, float gain,
                                     bool release) {
  float* frame;
  {
    std::lock_guard<std::mutex> guard(frames_mutex_);
    if (frames_queued_.empty()) {
      return false;
    }
    frame = frames_queued_.front();
    frames_queued_.pop();
  }
  std::array<float, kChannelSamples * 2> folded;
  conversion::sequential_6_BE_to_interleaved_2_LE(folded.data(), frame, kChannelSamples, fold,
                                                  gain);
  for (size_t i = 0; i < folded.size(); ++i) {
    stereo_out[i] += folded[i];
  }
  {
    std::lock_guard<std::mutex> guard(frames_mutex_);
    frames_unused_.push(frame);
  }
  // Exactly one release per frame really consumed: that is what lets the guest
  // render the next one. With audio_switch_pump, PumpTick requests them at a fixed rate.
  if (release) {
    auto released = semaphore_->Release(1, nullptr);
    assert_true(released);
  }
  return true;
}

size_t SwitchAudioDriver::QueuedFrames() {
  std::lock_guard<std::mutex> guard(frames_mutex_);
  return frames_queued_.size();
}

void SwitchAudioDriver::RequestFrame() {
  // The semaphore has a maximum: if the game does not serve the requests, the excess ones are lost.
  semaphore_->Release(1, nullptr);
}

/* --- system ------------------------------------------------------------------ */

struct SwitchAudioSystem::Output {
  Thread thread{};
  bool thread_created = false;
  std::atomic<bool> running{false};
  bool pump = false;  // audio_switch_pump, read when audio is opened
  size_t frames_in_queue = kTargetQueuedFrames;  // audio_switch_frames_in_queue, read when audio is opened
  uint32_t buffer_frames = kMaxFramesPerBuffer;
  uint32_t buffer_count = 3;
  uint32_t buffer_bytes = kMaxBufferBytes;
  bool audout_initialized = false;
  bool audout_started = false;
  std::array<AudioOutBuffer, kMaxBufferCount> buffers{};
  std::array<s16*, kMaxBufferCount> samples{};
  std::array<float, kMaxBufferSamples * 2> mix{};
};

std::unique_ptr<AudioSystem> SwitchAudioSystem::Create(
    runtime::FunctionDispatcher* function_dispatcher) {
  return std::make_unique<SwitchAudioSystem>(function_dispatcher);
}

SwitchAudioSystem::SwitchAudioSystem(runtime::FunctionDispatcher* function_dispatcher)
    : AudioSystem(function_dispatcher) {}

SwitchAudioSystem::~SwitchAudioSystem() {
  StopOutput();
}

X_STATUS SwitchAudioSystem::CreateDriver([[maybe_unused]] size_t index,
                                         rex::thread::Semaphore* semaphore,
                                         AudioDriver** out_driver) {
  assert_not_null(out_driver);
  // The device opens with the first client and then stays open, playing
  // silence while nobody renders.
  if (!output_ && !StartOutput()) {
    return X_STATUS_UNSUCCESSFUL;
  }
  auto driver = new SwitchAudioDriver(memory_, semaphore);
  {
    std::lock_guard<std::mutex> guard(drivers_mutex_);
    drivers_.push_back(driver);
  }
  *out_driver = driver;
  return X_STATUS_SUCCESS;
}

void SwitchAudioSystem::DestroyDriver(AudioDriver* driver) {
  assert_not_null(driver);
  auto switch_driver = static_cast<SwitchAudioDriver*>(driver);
  {
    std::lock_guard<std::mutex> guard(drivers_mutex_);
    drivers_.erase(std::remove(drivers_.begin(), drivers_.end(), switch_driver), drivers_.end());
  }
  delete switch_driver;
}

bool SwitchAudioSystem::StartOutput() {
  auto output = std::make_unique<Output>();
  output->pump = REXCVAR_GET(audio_switch_pump);
  output->frames_in_queue = static_cast<size_t>(std::clamp(REXCVAR_GET(audio_switch_frames_in_queue), 2, 32));
  output->buffer_frames = static_cast<uint32_t>(std::clamp(REXCVAR_GET(audio_switch_buffer_frames), 1, 4));
  output->buffer_count = static_cast<uint32_t>(std::clamp(REXCVAR_GET(audio_switch_buffers), 2, 4));
  output->buffer_bytes = output->buffer_frames * SwitchAudioDriver::kChannelSamples * 2 * sizeof(s16);
  REXLOG_INFO("SwitchAudioSystem: 187.5 Hz pump {}, {} frames queued, {} buffers of {} frames ({:.1f} ms each)",
              output->pump ? "active" : "off", output->frames_in_queue, output->buffer_count,
              output->buffer_frames, output->buffer_frames * 256.0 / 48.0);

  Result rc = audoutInitialize();
  if (R_FAILED(rc)) {
    REXLOG_ERROR("SwitchAudioSystem: audoutInitialize failed: 0x{:08X}", rc);
    return false;
  }
  output->audout_initialized = true;
  REXLOG_INFO("SwitchAudioSystem: audout {} Hz, {} channels, PCM format {}",
              audoutGetSampleRate(), audoutGetChannelCount(), int(audoutGetPcmFormat()));
  if (audoutGetSampleRate() != 48000 || audoutGetChannelCount() != 2 ||
      audoutGetPcmFormat() != PcmFormat_Int16) {
    REXLOG_ERROR("SwitchAudioSystem: unexpected audout format");
    audoutExit();
    return false;
  }

  for (size_t i = 0; i < output->buffer_count; ++i) {
    output->samples[i] = static_cast<s16*>(std::aligned_alloc(0x1000, kBufferAllocBytes));
    if (!output->samples[i]) {
      REXLOG_ERROR("SwitchAudioSystem: out of memory for audio buffers");
      output_ = std::move(output);
      StopOutput();
      return false;
    }
    std::memset(output->samples[i], 0, kBufferAllocBytes);
    AudioOutBuffer& buffer = output->buffers[i];
    buffer.next = nullptr;
    buffer.buffer = output->samples[i];
    buffer.buffer_size = kBufferAllocBytes;
    buffer.data_size = output->buffer_bytes;
    buffer.data_offset = 0;
  }

  rc = audoutStartAudioOut();
  if (R_FAILED(rc)) {
    REXLOG_ERROR("SwitchAudioSystem: audoutStartAudioOut failed: 0x{:08X}", rc);
    output_ = std::move(output);
    StopOutput();
    return false;
  }
  output->audout_started = true;

  output_ = std::move(output);
  output_->running = true;
  rc = threadCreate(
      &output_->thread,
      [](void* arg) { static_cast<SwitchAudioSystem*>(arg)->OutputThreadMain(); }, this, nullptr,
      kOutputThreadStack, kOutputThreadPriority, -2);
  if (R_FAILED(rc)) {
    REXLOG_ERROR("SwitchAudioSystem: threadCreate failed: 0x{:08X}", rc);
    output_->running = false;
    StopOutput();
    return false;
  }
  output_->thread_created = true;
  rc = threadStart(&output_->thread);
  if (R_FAILED(rc)) {
    REXLOG_ERROR("SwitchAudioSystem: threadStart failed: 0x{:08X}", rc);
    output_->running = false;
    StopOutput();
    return false;
  }
  return true;
}

void SwitchAudioSystem::StopOutput() {
  if (!output_) {
    return;
  }
  output_->running = false;
  if (output_->thread_created) {
    threadWaitForExit(&output_->thread);
    threadClose(&output_->thread);
  }
  if (output_->audout_started) {
    audoutStopAudioOut();
  }
  if (output_->audout_initialized) {
    audoutExit();
  }
  for (s16* samples : output_->samples) {
    if (samples) {
      std::free(samples);
    }
  }
  output_.reset();
}

void SwitchAudioSystem::FillAndAppend(size_t buffer_index) {
  Output& output = *output_;
  output.mix.fill(0.0f);
  // Snapshot once, so a change mid-buffer cannot split it across two mixes.
  const StereoFold fold = GetStereoFold();
  const float gain = GetOutputGain();
  uint32_t frames_mixed = 0;
  uint32_t frames_missing = 0;
  uint32_t queued_samples = 0;
  uint32_t samples_count = 0;
  {
    std::lock_guard<std::mutex> guard(drivers_mutex_);
    for (uint32_t frame = 0; frame < output.buffer_frames; ++frame) {
      float* out = output.mix.data() + frame * SwitchAudioDriver::kChannelSamples * 2;
      for (SwitchAudioDriver* driver : drivers_) {
        queued_samples += static_cast<uint32_t>(driver->QueuedFrames());
        ++samples_count;
        // A client with nothing queued contributes silence and gets no
        // release, exactly as the SDL driver does on underrun.
        if (driver->MixFrameInto(out, fold, gain, !output.pump)) {
          ++frames_mixed;
        } else {
          ++frames_missing;
        }
      }
    }
  }
  RexSwitchPerfAdd(28, queued_samples);
  RexSwitchPerfAdd(29, samples_count);

  s16* samples = output.samples[buffer_index];
  bool pcm_nonzero = false;
  // How much the mix saturates before it is clipped (the crackling in handheld mode). Counting only.
  float pico = 0.0f;
  uint32_t saturated = 0;
  uint32_t non_finite = 0;  // fork (shh-nx, 2026-10-29): NaN or infinite samples, played as silence
  const uint32_t mix_samples = output.buffer_frames * SwitchAudioDriver::kChannelSamples * 2;
  if (REXCVAR_GET(audio_mute)) {
    std::memset(samples, 0, output.buffer_bytes);
  } else {
    for (size_t i = 0; i < mix_samples; ++i) {
      const bool finite = std::isfinite(output.mix[i]);
      non_finite += finite ? 0 : 1;
      const float value = finite ? output.mix[i] : 0.0f;
      const float magnitude = std::fabs(value);
      pico = std::max(pico, magnitude);
      saturated += magnitude > 1.0f ? 1 : 0;
      samples[i] = static_cast<s16>(std::lrint(std::clamp(value, -1.0f, 1.0f) * 32767.0f));
      pcm_nonzero |= samples[i] != 0;
    }
  }
  RexSwitchPerfAdd(21, saturated);
  RexSwitchPerfAdd(30, non_finite);
  // Fork (shh-nx, 2026-10-29, SHH-12): a line every 2 s of whether the output had sound, next to the log's file
  // opens (the 10 s profile is too coarse to place a video).
  {
    static uint32_t buffers = 0, with_sound = 0;
    static float peak = 0.0f;
    static auto since = std::chrono::steady_clock::now();
    ++buffers;
    with_sound += pcm_nonzero ? 1 : 0;
    peak = std::max(peak, pico);
    const auto now = std::chrono::steady_clock::now();
    if (now - since >= std::chrono::seconds(2)) {
      REXLOG_INFO("[audio] 2 s: {}/{} buffers with sound, peak {:.3f}", with_sound, buffers, peak);
      buffers = with_sound = 0;
      peak = 0.0f;
      since = now;
    }
  }
  RexSwitchPerfAdd(22, pico >= 0.98f ? 1 : 0);
  RexSwitchPerfMax(23, static_cast<u64>(std::min(pico, 100.0f) * 10000.0f));
  output.buffers[buffer_index].data_size = output.buffer_bytes;
  output.buffers[buffer_index].data_offset = 0;
  Result rc = audoutAppendAudioOutBuffer(&output.buffers[buffer_index]);
  RexSwitchPerfAdd(24, frames_mixed);
  RexSwitchPerfAdd(25, frames_missing);
  if (R_SUCCEEDED(rc)) {
    RexSwitchPerfAdd(26, pcm_nonzero ? 1 : 0);
    RexSwitchPerfAdd(27, 1);
  }
  if (R_FAILED(rc)) {
    REXLOG_ERROR("SwitchAudioSystem: audoutAppendAudioOutBuffer failed: 0x{:08X}", rc);
  }
}

void SwitchAudioSystem::PumpTick() {
  std::lock_guard<std::mutex> guard(drivers_mutex_);
  for (SwitchAudioDriver* driver : drivers_) {
    const size_t queued = driver->QueuedFrames();
    if (queued <= output_->frames_in_queue) {
      driver->RequestFrame();
    }
    if (queued < kMinQueuedFrames) {
      driver->RequestFrame();  // the queue has emptied: one more to catch up
    }
  }
}

void SwitchAudioSystem::OutputThreadMain() {
  Output& output = *output_;
  for (size_t i = 0; i < output.buffer_count; ++i) {
    FillAndAppend(i);
  }
  const u64 interval = armNsToTicks(kPumpIntervalNs);
  u64 deadline = armGetSystemTick() + interval;
  while (output.running.load(std::memory_order_relaxed)) {
    u64 timeout_ns = kWaitTimeoutNs;
    if (output.pump) {
      const u64 now = armGetSystemTick();
      if (now >= deadline) {
        PumpTick();
        deadline += interval;
        if (now >= deadline) {
          deadline = now + interval;  // more than one tick late: no catch-up burst
        }
        continue;
      }
      timeout_ns = armTicksToNs(deadline - now);  // at most until the next deadline
    }
    AudioOutBuffer* released = nullptr;
    u32 released_count = 0;
    Result rc = audoutWaitPlayFinish(&released, &released_count, timeout_ns);
    // libnx hands back one released buffer per call; drain the rest so none is
    // left out of the rotation.
    while (R_SUCCEEDED(rc) && released && released_count) {
      const ptrdiff_t index = released - output.buffers.data();
      if (index >= 0 && index < ptrdiff_t(output.buffer_count)) {
        FillAndAppend(size_t(index));
      }
      released = nullptr;
      released_count = 0;
      rc = audoutGetReleasedAudioOutBuffer(&released, &released_count);
    }
  }
}

}  // namespace rex::audio::nx
