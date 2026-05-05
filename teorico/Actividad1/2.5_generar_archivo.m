clear all;
close all;
clc;

A = 1;
f = 10;
fs = 200;
N = 100;

n = 0:N-1;
t = n / fs;

x = A * sin(2*pi*f*t);

% Escalamos a enteros, por ejemplo int16
x_int = round(x * 32767);

archivo = fopen('senal.h', 'w');

fprintf(archivo, '#ifndef SENAL_H\n');
fprintf(archivo, '#define SENAL_H\n\n');

fprintf(archivo, '#define N_MUESTRAS %d\n\n', N);

fprintf(archivo, 'short senal[%d] = {\n', N);

for i = 1:N
    if i < N
        fprintf(archivo, '  %d,\n', x_int(i));
    else
        fprintf(archivo, '  %d\n', x_int(i));
    endif
endfor

fprintf(archivo, '};\n\n');
fprintf(archivo, '#endif\n');

fclose(archivo);

disp('Archivo senal.h generado correctamente');