# Progetto SOA 2021/2022 - Multi-Flow Device File
Dobbiamo implementare un Linux Device Driver. Dobbiamo creare le file operations. Che dobbiamo farci con ste file operations? Un multiflowdevice file. Che è sto coso?

Oggetto verso cui abbiamo due stream di byte. Uno stream low ed uno high. Nella high prio faremo alcune cose nella low altre.

Entrambi gli stream sono FIFO. Entrano leggiamo escono nello stesso ordine. La cosa interessante è che in questi due flussi lavoriamo secondo schemi differenti.

High Priority le scritture sono sincrone. Un thread chiama una write la write deve mettere i dati li ora quindi il thread non puo riprendere il controllo finche la write non è conclusa

Low Priority invece abbiamo una write delayed. I dati vengono presi ma messi piu avanti. I dati vanno quindi in una staging area e processata piu avanti. Piu avanti come lo decidiamo noi, abbiamo i diversi meccanismi di delayed work e noi scegliamo il nostro in base ai nostri motivi obiettivi.

Quando andiamo a scrivere dei dati su questo oggetto usando low priority la notifica della scrittura è comunque sincrona altrimenti andiamo contro le API di read di linux. Quindi prendo i dati li metto in staging area ma comunque notifico sincrono che li ho presi

Abbiamo minor number 128 quindi...

Con ioctl possiamo settare l'operatività della sessione di I/O su questo file. Settiamo l'operatività della sessione

- se dobbiamo andare high o low
-  Operazioni bloccanti oppure no
- Timeout per sbloccare le operazioni bloccate

Ovviamente abbiamo anche altre info da inserire.

Parametr per acquisire. Capire se il multiflow è abilitato o disabilitato, i byte sui due flussi e quanti thread sono sui due flussi. Quindi ci sono delle variabili che possiamo controllare sul VFS che ci dicono che possiamo abilitare o disabilitare uno specifico minor number.

Quindi mischiamo VFS gestione dei thread schedulazione parametri configurazione dinamica degli oggetti.

Ne possiamo discutere comunque per email o a ricevimento.