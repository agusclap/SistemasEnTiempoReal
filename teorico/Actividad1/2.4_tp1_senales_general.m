clear all;
close all;
clc;

A = 1;
f = 10;
fs = 200;
N = 200;

n = 0:N-1;
t = n / fs;

x = A * sin(2*pi*f*t);

% FFT
X = fft(x);

% Eje de frecuencias
frecuencias = (0:N-1) * fs / N;

% Magnitud normalizada
magnitud = abs(X) / N;

figure;
plot(frecuencias, magnitud);
grid on;
title('Espectro de frecuencia usando FFT');
xlabel('Frecuencia [Hz]');
ylabel('Magnitud');