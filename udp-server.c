/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

#include "contiki.h"
#include "net/routing/routing.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"

// [GELİŞTİRME ADIMI 1]: Kalıcı Depolama (Disk) Kütüphanesi
// Gelen firmware paketlerini cihazın ROM/Flash hafızasına yazmak için 
// Contiki-NG'nin Coffee File System (CFS) kütüphanesini dahil ettik.
#include "cfs/cfs.h"

#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#define WITH_SERVER_REPLY  1
#define UDP_CLIENT_PORT 8765
#define UDP_SERVER_PORT 5678

// [GELİŞTİRME ADIMI 2]: Gönderici ile Ortak Kargo Kutusu (Struct)
// Gelen veri paketlerini doğru şekilde ayrıştırabilmek için 
// göndericideki kargo kutusunun aynısını depocuya da (Server) tanımladık.
struct ota_packet {
  uint16_t block_num; // Paketin sırası 
  uint16_t data_len;  // İçindeki verinin boyutu
  uint8_t data[64];   // Gelen asıl firmware parçası
};

static struct simple_udp_connection udp_conn;

PROCESS(udp_server_process, "UDP server");
AUTOSTART_PROCESSES(&udp_server_process);
/*---------------------------------------------------------------------------*/

static void
udp_rx_callback(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{
  // [GELİŞTİRME ADIMI 3]: Ham Veriyi Yapılandırma (Type Casting)
  // Ağdan gelen 'data' dizisini, bizim tanımladığımız 'ota_packet' formatına çeviriyoruz.
  struct ota_packet *packet = (struct ota_packet *)data;

  LOG_INFO("Kargo Geldi! Blok No: %u, Veri Boyutu: %u bayt. Gonderen: ", packet->block_num, packet->data_len);
  LOG_INFO_6ADDR(sender_addr);
  LOG_INFO_("\n");

  // --- [GELİŞTİRME ADIMI 4]: CFS İle Diske Yazma (Şartname Gereksinimi) ---
  // Gelen blokları geçici hafızada (RAM) tutmak yerine doğrudan diske yazıyoruz.
  // Dosyayı CFS_WRITE modunda açıyoruz (yoksa otomatik oluşturulur).
  int fd = cfs_open("yeni_firmware.bin", CFS_WRITE);
  
  if(fd >= 0) {
      // Paketlerin karışık gelme veya diske yanlış yazılma ihtimaline karşı,
      // blok numarasına göre dosyadaki tam konumuna (offset) gidiyoruz.
      cfs_seek(fd, packet->block_num * 64, CFS_SEEK_SET);
      
      // Asıl firmware verisini diske kalıcı olarak işliyoruz.
      cfs_write(fd, packet->data, packet->data_len);
      
      // İşlem bitince hafıza sızıntısını (memory leak) önlemek için dosyayı kapatıyoruz.
      cfs_close(fd);
      LOG_INFO("-> Kargo diske (CFS) basariyla islendi.\n");
  } else {
      LOG_ERR("-> HATA: Diske erisim saglanamadi!\n");
  }

  // --- [GELİŞTİRME ADIMI 5]: Bitiş Kontrolü ---
  // 129.760 baytlık dosya 64 baytlık bloklara bölündüğünde son indeks 2027 olur.
  // Şartnamedeki "alımı tamamlandı mesajı yazdırılmalıdır" kuralı işletiliyor.
  if(packet->block_num == 2027) {
      LOG_INFO("Yuklenmeye hazir yeni firmware alimi tamamlandi.\n");
  }

#if WITH_SERVER_REPLY
  // [GELİŞTİRME ADIMI 6]: Stop-and-Wait Onay Mekanizması (ACK)
  // Göndericiye (Client), "Bu paketi başarıyla diske yazdım, sıradakini yolla" diyoruz.
  simple_udp_sendto(&udp_conn, data, datalen, sender_addr);
#endif /* WITH_SERVER_REPLY */
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_server_process, ev, data)
{
  PROCESS_BEGIN();

  /* Initialize DAG root */
  NETSTACK_ROUTING.root_start();

  /* Initialize UDP connection */
  simple_udp_register(&udp_conn, UDP_SERVER_PORT, NULL,
                      UDP_CLIENT_PORT, udp_rx_callback);

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/