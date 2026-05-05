clear all;
close all;
clc;

A = 2;        % Amplitud
f = 10;       % Frecuencia de la señal en Hz
fs = 200;     % Frecuencia de muestreo en Hz
N = 200;      % Cantidad de muestras

n = 0:N-1;
t = n / fs;

x = A * sin(2*pi*f*t);

figure;
plot(t, x, 'o-');
grid on;
title('Señal sinusoidal digital');
xlabel('Tiempo [s]');
ylabel('Amplitud');