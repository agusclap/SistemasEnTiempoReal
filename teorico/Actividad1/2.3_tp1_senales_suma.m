clear all;
close all;
clc;

A = 1;
f1 = 5;
f2 = 20;
fs = 200;
N = 200;

n = 0:N-1;
t = n / fs;

x1 = A * sin(2*pi*f1*t);
x2 = A * sin(2*pi*f2*t);

x_total = x1 + x2;

figure;
plot(t, x1);
grid on;
title('Señal 1');
xlabel('Tiempo [s]');
ylabel('Amplitud');

figure;
plot(t, x2);
grid on;
title('Señal 2');
xlabel('Tiempo [s]');
ylabel('Amplitud');

figure;
plot(t, x_total);
grid on;
title('Suma de dos señales sinusoidales');
xlabel('Tiempo [s]');
ylabel('Amplitud');