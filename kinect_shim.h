// kinect_shim.h — Interfaz tipo-C para Kinect v1 (de pie)

#ifndef KINECT_SHIM_H
#define KINECT_SHIM_H

#ifdef __cplusplus
extern "C" {
#endif

// Inicializa Kinect (esqueleto). Devuelve 1 si OK, 0 si falla.
int ks_init(void);

// Cierra Kinect.
void ks_shutdown(void);

// Actualiza datos; devuelve 1 si hay mano derecha trackeada.
int ks_update(void);

// Obtiene mano derecha normalizada 0..1 en X,Y y Z (metros desde sensor aprox).
// Solo válido si ks_update() devolvió 1.
void ks_get_right_hand(float* x, float* y, float* z);

// Habilita/ajusta suavizado [0..1]; 0 = sin suavizado, 0.5 recomendado.
void ks_set_smoothing(float smoothing, float correction, float prediction,
                      float jitter_radius, float max_deviation);

#ifdef __cplusplus
}
#endif
#endif // KINECT_SHIM_H
