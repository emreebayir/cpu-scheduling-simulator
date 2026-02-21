# CPU-SCHEDULER-SIMULATION
İşletim Sistemi Zamanlayıcı ve Kaynak Simülatörü (OS Scheduler Simulator)
Bu proje, bir işletim sisteminin çekirdeğinde yer alan CPU Zamanlama (CPU Scheduling) algoritmalarını ve paylaşılan kaynaklar için Senkronizasyon/Deadlock Yönetimi süreçlerini "tick" (adım) tabanlı olarak modelleyen bir simülatördür. C++17 standartlarına uygun olarak geliştirilmiştir.

🚀 Özellikler
1. Zamanlama Algoritmaları
Simülatör, süreçleri READY, BLOCKED, RUNNING ve TERMINATED durumları arasında yönetir ve şu algoritmaları destekler:

Round Robin (RR): Preemptive yapıdadır; süreçler komut satırından verilen quantum süresi kadar çalışır.

Priority Preemptive: Önceliğe dayalı çalışır; Starvation'ı (açlığı) önlemek için Aging (Yaşlandırma) mekanizması zorunlu olarak uygulanır.

Multi-Level Feedback Queue (MLFQ): 3 farklı seviye kuyruk içerir. Her seviye için farklı quantum değerleri kullanılır ve alt seviyelerde kilitlenmeyi engellemek için periyodik Priority Boost uygulanır.

2. Kaynak Yönetimi ve Senkronizasyon
Sistemde kapasiteleri önceden tanımlı M adet kaynak (R1..RM) bulunur.

Süreçler çalışma esnasında kaynak talep edebilir (REQ) veya işi bitince bırakabilir (REL).

İstenen kaynak o an müsait değilse, süreç BLOCKED durumuna geçer. Kaynak serbest kaldığında ilgili süreç READY kuyruğuna geri döner.

3. Deadlock Tespit ve Kurtarma (Detection & Recovery)
Deadlock Tespiti: Sistemdeki kaynak beklemeleri bir Wait-For Graph olarak modellenir. Her adımda grafikte bir döngü (cycle) olup olmadığı kontrol edilir.

Deadlock Kurtarma: Bir kilitlenme tespit edilirse, döngüdeki süreçlerden biri kurban seçilerek sonlandırılır (Abort) ve tuttuğu tüm kaynaklar sisteme iade edilir.

Kurulum ve Derleme
Proje C++17 standardında yazılmıştır. Derlemek için terminale şu komutu yazabilirsiniz:
g++ -std=c++17 main.cpp -o scheduler

Çalıştırma ve Kullanım
Programı çalıştırırken şu komut satırı argümanları kullanılır:
./scheduler --alg <rr|prio|mlfq> --q <quantum> --input <dosya_yolu>

Parametreler:

--alg: Kullanılacak algoritma (rr, prio veya mlfq).

--q: RR ve MLFQ için quantum süresi.

--input: İş yükü senaryosunu içeren dosya yolu.

Çıktılar ve Raporlama
Simülasyon sonunda program şu verileri üretir:

Timeline Log: Her CPU ataması, bloklanma ve kilitlenme çözme olaylarının zaman çizelgesi.

Performans Metrikleri:

Ortalama Bekleme Süresi (Average Waiting Time)

Ortalama Dönüş Süresi (Average Turnaround Time)

CPU Kullanım Oranı (CPU Utilization) ve Throughput

Deadlock Raporu: Oluşan deadlock'lar, dahil olan süreçler ve uygulanan kurtarma işlemleri.
