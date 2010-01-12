#ifndef __CLOCK_H
#define __CLOCK_H

/** time_val speichert die Zeit in Mikrosekunden (us) seit dem 1.1.1970 */
typedef long long time_val;

/** Initialisiert die Uhr mit zufaelligen Offset und Driftrate */
extern "C" void clock_init();

/** @brief Initialisiert die Uhr mit vorgegebenen Offset und Driftrate
 *
 * Die virtuelle Uhr wird mit vorbestimmter Abweichung initialisert.
 *
 * Beispiel für +1s Offset und 10% Driftrate:
 * 	clock_setup(1000000, 110);
 *
 * @param t_offset Offset der Uhr [us]
 * @param speed_pct Rate der Uhr [%]
 */
extern "C" void clock_setup(time_val t_offset, time_val speed_pct);

/** @brief Liefert die (virtuelle) Zeit zurück
 *
 * @return Zeit seit 1.1.1970 [us]
 */
extern "C" time_val get_time();

/** @brief Passt die lokale Uhr an
 *
 * @param diff Zeitverschiebung in die Zukunft [us]
 */
extern "C" void adjust_time(time_val diff);

/** @brief Liefert die Wartezeit für poll() bis zum angegebenen Zeitpunkt
 *
 * @param abstime Zeitpunkt bis zu dem gewartet werden soll
 * @return poll()-Wartezeit [ms]
 */
extern "C" int poll_time(time_val abstime);

/** @brief Speichert einen Zeitwert ins Netzwerkformat
 *
 * @param tv Zeitwert
 * @param addr Adresse des Speicherbereichs (muss 8 Bytes Platz bieten)
 */
extern "C" void time2net(time_val tv, char * addr);

/** @brief Liest einen Zeitwert aus einem Netzwerkpaket
 *
 * @param addr Adresse des Speicherbereichs (mit einem 8-Byte-Datenwert)
 * @return Zeitwert
 */
extern "C" time_val net2time(char * addr);

#endif // __CLOCK_H
