clear all;
close all;
clc;

% Parámetros de la señal
A = 1;              % Amplitud
f = 5;              % Frecuencia de la señal en Hz
fs = 100;           % Frecuencia de muestreo en Hz
N = 100;            % Cantidad de muestras

n = 0:N-1;
t = n / fs;

% Señal sinusoidal
x = A * sin(2*pi*f*t);

% Cuantización con distintas resoluciones
bits1 = 4;
bits2 = 8;
bits3 = 12;

niveles1 = 2^bits1;
niveles2 = 2^bits2;
niveles3 = 2^bits3;

xq1 = round((x + A) * (niveles1 - 1) / (2*A)) * (2*A) / (niveles1 - 1) - A;
xq2 = round((x + A) * (niveles2 - 1) / (2*A)) * (2*A) / (niveles2 - 1) - A;
xq3 = round((x + A) * (niveles3 - 1) / (2*A)) * (2*A) / (niveles3 - 1) - A;

figure;
plot(t, x, 'k', t, xq1, 'o-');
grid on;
title('Señal sinusoidal cuantizada con 4 bits');
xlabel('Tiempo [s]');
ylabel('Amplitud');

figure;
plot(t, x, 'k', t, xq2, 'o-');
grid on;
title('Señal sinusoidal cuantizada con 8 bits');
xlabel('Tiempo [s]');
ylabel('Amplitud');

figure;
plot(t, x, 'k', t, xq3, 'o-');
grid on;
title('Señal sinusoidal cuantizada con 12 bits');
xlabel('Tiempo [s]');
ylabel('Amplitud');