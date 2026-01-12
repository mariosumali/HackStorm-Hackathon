import sys
import serial
import serial.tools.list_ports
import numpy as np
import pyqtgraph as pg
from PyQt6.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, 
                             QHBoxLayout, QComboBox, QPushButton, QLabel, QMessageBox, QFileDialog)
from PyQt6.QtCore import QThread, pyqtSignal, QBuffer, QIODevice
from PyQt6.QtMultimedia import QMediaPlayer, QAudioOutput, QAudioSource, QAudioFormat, QAudioDevice 
# Note: PyQt6.QtMultimedia is a bit complex for raw PCM. We will use sounddevice or simpleaudio if available,
# but to stick to the installed packages (numpy, pyqt6), we can write a generic WAV file and play it via system or QMediaPlayer.

import time
import wave
import struct
import tempfile
import os

# Configuration
BAUD_RATE = 921600
SAMPLE_RATE = 16000
CHANNELS = 1
PCM_BITS = 16
MAX_BUFFER_SIZE = 10 * 1024 * 1024 # Limit to 10MB to avoid crazy memory usage

class SerialWorker(QThread):
    status_updated = pyqtSignal(str)
    upload_started = pyqtSignal()
    data_received = pyqtSignal(bytes)
    upload_finished = pyqtSignal()

    def __init__(self, port):
        super().__init__()
        self.port = port
        self.running = True

    def run(self):
        try:
            self.ser = serial.Serial(self.port, BAUD_RATE, timeout=0.1)
            self.status_updated.emit(f"Connected to {self.port}")
            
            is_uploading = False

            while self.running:
                try:
                    line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                    
                    if line == "UPLOAD_START":
                        is_uploading = True
                        self.upload_started.emit()
                        self.status_updated.emit("Receiving data...")
                        continue
                    
                    if line == "UPLOAD_END":
                        is_uploading = False
                        self.upload_finished.emit()
                        self.status_updated.emit("Upload Complete")
                        continue

                    if is_uploading and line.startswith("AUDIO:"):
                        hex_str = line[6:]
                        try:
                            audio_data = bytes.fromhex(hex_str)
                            self.data_received.emit(audio_data)
                        except ValueError:
                            pass 

                except serial.SerialException:
                    self.status_updated.emit("Serial Error")
                    break
        except serial.SerialException as e:
            self.status_updated.emit(f"Failed to connect: {str(e)}")

    def stop(self):
        self.running = False
        self.wait()

class AudioVisualizer(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Tuya Recorder - Capture & Review")
        self.resize(1200, 800)

        # Main Layout
        self.central_widget = QWidget()
        self.setCentralWidget(self.central_widget)
        self.layout = QVBoxLayout(self.central_widget)

        # Controls
        self.controls_layout = QHBoxLayout()
        self.port_selector = QComboBox()
        self.refresh_ports()
        self.connect_btn = QPushButton("Connect")
        self.connect_btn.clicked.connect(self.toggle_connection)
        
        self.status_label = QLabel("Status: Disconnected")
        
        self.play_btn = QPushButton("Play Recording")
        self.play_btn.clicked.connect(self.play_audio)
        self.play_btn.setEnabled(False)

        self.save_btn = QPushButton("Save WAV")
        self.save_btn.clicked.connect(self.save_wav)
        self.save_btn.setEnabled(False)
        
        self.controls_layout.addWidget(QLabel("Port:"))
        self.controls_layout.addWidget(self.port_selector)
        self.controls_layout.addWidget(self.connect_btn)
        self.controls_layout.addWidget(ViewSpacer())
        self.controls_layout.addWidget(self.play_btn)
        self.controls_layout.addWidget(self.save_btn)
        self.controls_layout.addWidget(ViewSpacer())
        self.controls_layout.addWidget(self.status_label)
        
        self.layout.addLayout(self.controls_layout)

        # Plots
        self.wave_plot = pg.PlotWidget(title="Full Recording Waveform")
        self.wave_plot.setYRange(-32768, 32768)
        self.wave_plot.showGrid(x=True, y=True)
        self.wave_curve = self.wave_plot.plot(pen='c')
        self.layout.addWidget(self.wave_plot)

        # State
        self.worker = None
        self.pcm_data = bytearray()
        
        # Audio Player
        self.player = QMediaPlayer()
        self.audio_output = QAudioOutput()
        self.player.setAudioOutput(self.audio_output)
        self.audio_output.setVolume(1.0)
        
        # Temp file for playback
        self.temp_wav_path = None

        self.apply_styles()

    def apply_styles(self):
        self.setStyleSheet("""
            QMainWindow { background-color: #2b2b2b; color: #ffffff; }
            QLabel { color: #ffffff; font-size: 14px; }
            QPushButton { 
                background-color: #3d3d3d; 
                color: #ffffff; 
                border: 1px solid #555;
                padding: 6px 15px;
                border-radius: 4px;
                font-weight: bold;
            }
            QPushButton:hover { background-color: #4d4d4d; }
            QPushButton:disabled { background-color: #333; color: #777; }
            QComboBox { background-color: #3d3d3d; color: #ffffff; border: 1px solid #555; padding: 5px; }
        """)

    def refresh_ports(self):
        self.port_selector.clear()
        ports = serial.tools.list_ports.comports()
        for port in ports:
            self.port_selector.addItem(port.device)

    def toggle_connection(self):
        if self.worker is None:
            port = self.port_selector.currentText()
            if not port: return
            
            self.worker = SerialWorker(port)
            self.worker.status_updated.connect(self.update_status)
            self.worker.upload_started.connect(self.handle_upload_start)
            self.worker.data_received.connect(self.handle_data)
            self.worker.upload_finished.connect(self.handle_upload_finish)
            self.worker.start()
            
            self.connect_btn.setText("Disconnect")
            self.port_selector.setEnabled(False)
        else:
            self.worker.stop()
            self.worker = None
            self.connect_btn.setText("Connect")
            self.port_selector.setEnabled(True)
            self.status_label.setText("Status: Disconnected")

    def update_status(self, msg):
        self.status_label.setText(f"Status: {msg}")

    def handle_upload_start(self):
        self.pcm_data = bytearray()
        self.play_btn.setEnabled(False)
        self.save_btn.setEnabled(False)
        self.wave_curve.setData([])

    def handle_data(self, data):
        self.pcm_data.extend(data)
        # Optional: Update plot partially if you want animation, 
        # but for bulk upload, waiting for end is usually fine or update every N bytes.
        if len(self.pcm_data) % 16000 == 0: # Update every ~0.5s of audio
             self.update_plot()

    def handle_upload_finish(self):
        self.update_plot()
        self.play_btn.setEnabled(True)
        self.save_btn.setEnabled(True)
        self.create_temp_wav()

    def update_plot(self):
        if not self.pcm_data: return
        count = len(self.pcm_data) // 2
        fmt = f"<{count}h" 
        try:
            samples = struct.unpack(fmt, self.pcm_data[:count*2])
            samples_np = np.array(samples, dtype=np.int16)
            self.wave_curve.setData(samples_np)
        except Exception:
            pass

    def create_temp_wav(self):
        if self.temp_wav_path and os.path.exists(self.temp_wav_path):
            os.unlink(self.temp_wav_path)
            
        fd, self.temp_wav_path = tempfile.mkstemp(suffix=".wav")
        with os.fdopen(fd, 'wb') as wav_file:
            with wave.open(wav_file, 'wb') as wf:
                wf.setnchannels(CHANNELS)
                wf.setsampwidth(2) # 16-bit
                wf.setframerate(SAMPLE_RATE)
                wf.writeframes(self.pcm_data)
        
        from PyQt6.QtCore import QUrl
        self.player.setSource(QUrl.fromLocalFile(self.temp_wav_path))

    def play_audio(self):
        if self.player.playbackState() == QMediaPlayer.PlaybackState.PlayingState:
            self.player.stop()
            self.play_btn.setText("Play Recording")
        else:
            self.player.play()
            self.play_btn.setText("Stop Playing")

    def save_wav(self):
        filename, _ = QFileDialog.getSaveFileName(self, "Save Audio", "", "WAV Files (*.wav)")
        if filename:
            if not filename.endswith(".wav"): filename += ".wav"
            try:
                with wave.open(filename, 'wb') as wf:
                    wf.setnchannels(CHANNELS)
                    wf.setsampwidth(2)
                    wf.setframerate(SAMPLE_RATE)
                    wf.writeframes(self.pcm_data)
                QMessageBox.information(self, "Success", f"Saved to {filename}")
            except Exception as e:
                QMessageBox.critical(self, "Error", str(e))

def ViewSpacer():
    s = QWidget()
    s.setFixedWidth(20)
    return s

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = AudioVisualizer()
    window.show()
    sys.exit(app.exec())
