#include "contiki.h"
#include "net/routing/routing.h"
#include "random.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

// [GELİŞTİRME ADIMI 1]: OTA Paketi - Kargo Kutumuz
// Şartnamedeki "sabit boyutlu paketlere bölünmelidir" maddesini sağlamak için
// 64 baytlık veri, sıra numarası ve boyut içeren bir yapı (struct) oluşturduk.
struct ota_packet {
  uint16_t block_num; // Paketin sırası (Örn: 1. paket, 2. paket)
  uint16_t data_len;  // İçindeki verinin boyutu
  uint8_t data[64];   // Gönderilecek asıl firmware parçası
};

#include "sys/node-id.h"
#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#define WITH_SERVER_REPLY  1
#define UDP_CLIENT_PORT 8765
#define UDP_SERVER_PORT 5678

// [GELİŞTİRME ADIMI 2]: Gönderim süresini hızlandırdık.
// 2000'den fazla paketin hızlıca gönderilmesi için aralığı 1 saniyeye indirdik.
#define SEND_INTERVAL     (1 * CLOCK_SECOND) 

static struct simple_udp_connection udp_conn;

// [GELİŞTİRME ADIMI 3]: Stop-and-Wait algoritması için sayacı global yaptık.
// Böylece udp_rx_callback fonksiyonu onay (ACK) geldiğinde bu sayacı artırabilecek.
static uint32_t tx_count = 0; 

/*---------------------------------------------------------------------------*/
PROCESS(udp_client_process, "UDP client");
AUTOSTART_PROCESSES(&udp_client_process);
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
  (void)c; (void)sender_port; (void)receiver_addr; (void)receiver_port;

  // [GELİŞTİRME ADIMI 4]: Yeniden Gönderim (Retransmission) Mekanizması
  // Depodan (Alıcıdan) geri dönen onay paketini okuyoruz.
  struct ota_packet *ack = (struct ota_packet *)data;

  LOG_INFO("-> ONAY (ACK) Geldi: %u. blok depoya ulasmis!\n", ack->block_num);

  // Eğer depodan gelen onay numarası bizim yolladığımız numarayla eşleşiyorsa,
  // paket kaybolmamış demektir. Sıradaki pakete geçmek için sayacı artırıyoruz.
  if(ack->block_num == tx_count) {
      tx_count++; 
  }
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_client_process, ev, data)
{
  static struct etimer periodic_timer;
  static struct ota_packet packet;
  uip_ipaddr_t dest_ipaddr;
  static uint32_t missed_tx_count;

  PROCESS_BEGIN();

  /* Initialize UDP connection */
  simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL,
                      UDP_SERVER_PORT, udp_rx_callback);

  etimer_set(&periodic_timer, random_rand() % SEND_INTERVAL);
  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));

    if(NETSTACK_ROUTING.node_is_reachable() &&
        NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr)) {

      if(node_id == 2) {
        
        LOG_INFO("Sending request %" PRIu32 " to ", tx_count);
        LOG_INFO_6ADDR(&dest_ipaddr);
        LOG_INFO_("\n");

        // [GELİŞTİRME ADIMI 5]: 16-bit Hafıza Sınırını Aşma Yöntemi
        // 129 KB'lık dosyayı RAM'e sığdıramayacağımız için toplam boyutu belirleyip,
        // veriyi "offset" (konum) mantığıyla anlık olarak üretiyoruz.
        uint32_t total_file_size = 129760;
        uint32_t offset = tx_count * 64;

        // Dosya bitmediği sürece kargo paketlemeye devam et.
        if (offset < total_file_size) {
            uint16_t chunk_size = 64; 
            
            // Son pakette küsurat kaldıysa, sadece kalan boyutu hesapla.
            if (offset + 64 > total_file_size) {
                chunk_size = total_file_size - offset;
            }

            // Kutu etiketini (sıra ve boyut) doldur.
            packet.block_num = tx_count;
            packet.data_len = chunk_size;
            
            // Simülasyon gereği, gerçek dosya okumak yerine hafıza dostu ardışık veri üretiyoruz.
            for(int i = 0; i < chunk_size; i++) {
                packet.data[i] = (uint8_t)((offset + i) % 256);
            }
            
            LOG_INFO("Kargo yola cikti: Blok %" PRIu32 ", Boyut %u bayt, Dosya Konumu: %" PRIu32 "\n", tx_count, chunk_size, offset);
            
            // Hazırlanan struct (yapı) paketini ağ üzerinden gönder.
            simple_udp_sendto(&udp_conn, &packet, sizeof(packet), &dest_ipaddr);
            
            // DİKKAT: tx_count++ BURADAN KALDIRILDI. (Yeniden gönderim stratejisi gereği
            // sayı sadece udp_rx_callback fonksiyonunda ACK gelince artar).

        } else {
            // Şartnamede istenen "aktarım sonu" mesajı.
            LOG_INFO("MUHTESEM! 129 KB'lik tum firmware basariyla kargolandi. Toplam %" PRIu32 " blok gonderildi.\n", tx_count);
        }
      }

    } else {
      LOG_INFO("Not reachable yet\n");
      if(tx_count > 0) {
        missed_tx_count++;
      }
    }

    /* Add some jitter */
    etimer_set(&periodic_timer, SEND_INTERVAL
      - CLOCK_SECOND + (random_rand() % (2 * CLOCK_SECOND)));
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/