**<div align="center">Estación meteorológica</div>**
-------------

<br/>

<p align="center"><img src="http://drive.google.com/uc?export=view&id=1CCsWAGHXle8UsT4V-PWq-cXUSE_hGxUf" alt="Google Logo"></p>

<br/>

Estación meteorológica creada con el microcontrolador **ESP32** y los sensores **BME280**  y **MQ135** con lecturas de temperatura, humedad relativa, presión atmosférica y calidad del aire. Los datos se muestran en una pantalla LCD y son subidos a un servidor local creado por el propio microcontrolador, así como a la plataforma ThingSpeak para un mejor análisis de los datos. Dispone de un sensor de luz (una fotorresistencia) para conocer la iluminación del lugar y avisos al móvil mediante un bot de Telegram por lecturas como un aumento brusco de la temperatura o muy mala calidad del aire. El encendido de un LED rojo facilita la identificación visual de esta última situación.
