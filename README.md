# Contiki-NG OTA Firmware Güncelleme Projesi

## Gerçeklenen Yöntemler ve Alınan Önlemler
Bu projede 129 KB boyutundaki firmware imajı, kısıtlı hafıza (RAM) kapasitesine sahip cihazlar arasında aktarılabilmesi için parçalanarak gönderilmiştir.

* **Paket Uzunluğu:** Cihazların donanım sınırları göz önüne alınarak veriler **64 baytlık** sabit bloklara bölünmüştür. Her pakete `block_num` (sıra numarası) eklenerek sıralama güvence altına alınmıştır.
* **Kalıcı Depolama:** Alıcı düğüm (Server), gelen verileri RAM'de tutmak yerine Contiki-NG'nin CFS (Coffee File System) yapısını kullanarak doğrudan Flash/Disk belleğine yazacak şekilde tasarlanmıştır. 
* **Alınan Önlem (Kayıp Paket Kontrolü):** Ağ üzerinde veri kayıplarını önlemek için **Stop-and-Wait (Dur ve Bekle)** algoritması kodlanmıştır. Gönderici düğüm, alıcıdan ACK (teslim onayı) gelmeden bir sonraki pakete geçmez. Bu sayede yüzde yüz güvenilir bir aktarım sağlanmıştır.

## YouTube Video Linki
[VİDEO LİNKİNİ BURAYA YAPIŞTIRIN]
