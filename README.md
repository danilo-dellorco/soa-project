# Progetto Sistemi Operativi Avanzati 2021/2022 - Multiflow Device Driver   
## Specifica
La specifica è relativa a un device driver Linux che implementa flussi di dati a bassa e alta priorità. Attraverso una sessione aperta al device file un thread può leggere e scrivere segmenti di dati. La trasmissione dei dati segue una politica di First-in-First-out lungo ciascuno dei due diversi flussi di dati (bassa e alta priorità).

Dopo un’ operazione di lettura, i dati letti scompaiono dal flusso. Inoltre, il flusso di dati ad alta priorità deve offrire operazioni di scrittura sincrone, mentre il flusso di dati a bassa priorità deve offrire un'esecuzione asincrona (basata su delayed work) delle operazioni di scrittura, mantenendo comunque l'interfaccia in grado di notificare in modo sincrono il risultato. Le operazioni di lettura sono invece eseguite tutte in modo sincrono. 

Il driver del dispositivo dovrebbe supportare 128 dispositivi corrispondenti alla stessa quantità di minor number. Il driver del dispositivo dovrebbe implementare il supporto per il servizio ioctl() al fine di gestire la sessione di I/O come segue:
- Impostare il livello di priorità (alto o basso) per le operazioni.
- Operazioni di lettura e scrittura bloccanti o non bloccanti.
- Configurare un timeout che regoli il risveglio delle operazioni bloccanti.

Devono essere implementati anche alcuni parametri del modulo Linux ed alcune funzioni per abilitare o disabilitare il device file, in termini di uno specifico minor number. Se disabilitato, qualsiasi tentativo di aprire una sessione dovrebbe fallire (ma le sessioni già aperte saranno ancora gestite). Ulteriori parametri aggiuntivi esposti tramite VFS dovrebbero fornire un’immagine dello stato attuale del dispositivo in base alle seguenti informazioni:
- Abilitato o disabilitato.
- Numero di byte attualmente presenti nei due flussi (alta o bassa priorità).
- Numero di thread attualmente in attesa di dati lungo i due flussi (alta o bassa priorità).


## Organizzazione della Repository
La directory principale del progetto è `soa-project`, che mantiene al suo interno due directory driver e user.
- `driver/`: contiene il codice `multiflow_driver.c` del modulo e lo script reinstall_module.sh, che permette di compilare ed installare rapidamente il modulo. 
- `user/`: contiene il codice `user_cli.c` e l’eseguibile `user_cli` che implementa una semplice CLI per interagire con i dispositivi del driver.
- `doc/`: contiene la documentazione sul progetto

## Montaggio e Rimozione del Modulo
Per installare il modulo si può eseguire lo script `driver/reinstall_module.sh`. Nello specifico questo va a smontare eventuale versioni precedenti del modulo, compila l’ultima versione, e poi la installa nel sistema. 

In alternativa si possono comunque eseguire manualmente i comandi necessari, quindi `make all` per la compilazione, e `insmod multiflow_driver.ko` per l’installazione. 

Quando il modulo viene montato con successo sul buffer del kernel viene stampato il major number assegnatogli. Questo può essere quindi recuperato dall’utente tramite il comando `dmesg`. 

Per rimuovere il modulo si può utilizzare il comando `rmmod multiflow_driver`, mentre tramite `make clean` si possono rimuovere dalla directory soa-project/driver tutti i file generati in fase di compilazione.

## Utilizzo della User CLI
Lanciando tramite `sudo` il programma `user/user_cli` è possibile interagire con i multiflow devices tramite il driver appena installato. Il programma accetta due argomenti da riga di comando:
- `major` (`argv[1]`): Major number del device installato, ottenuto tramite dmesg.
- `device_path` (`argv[2]`): Percorso nel VFS in cui verranno installati i dispositivi. Se l’utente non specifica questo parametro viene utilizzato il path di default, ovvero `/dev/mflow-dev`.

Prima di operare con il Char Device è necessario creare i device file che rappresentano i dispositivi sul VFS. Questo può essere fatto: 
- Manualmente tramite `mknod dev/nome_device MAJOR MINOR`
- Utilizzando il comando 11 (*Create device nodes*) da `user_cli`, che genera automaticamente 128 file relativi ai dispositivi che devono essere gestiti.

### Operazioni sui device
La CLI offre le operazioni basilari per operare con un dispositivo.
- **Open a device file (0)**: Chiede all’utente di inserire un minor number `N` e viene aperto il relativo file `/dev/mflow-devN`. Dopo aver aperto un file vengono mostrate nell’header della CLI le informazioni principali su quel device.
- **Write on the device file (1)**: Richiede all’utente di inserire i dati da scrivere, ed la scrittura sul file aperto.
- **Read from the device file (2)**: Richiede all’utente la quantità di bytes che vuole leggere, ed effettua la lettura dal file aperto.

### Operazioni sulla sessione
Tramite CLI è possibile modificare alcuni parametri della sessione, che vanno a definire il comportamento delle operazioni di write/read.

- **Switch to LOW/HIGH priority (3/4)**: Modifica il parametro priority della sessione, cambiando quindi il flusso dati da HIGH a LOW o viceversa.
- **Use BLOCKING/NON-BLOCKING operations (5/6)**: Viene modificato il parametro blocking della sessione, passando quindi da operazioni non-bloccanti a bloccanti e viceversa.
- **Set timeout (7)**: Modifica il parametro timeout della sessione, impostando quindi il tempo di attesa per il lock nelle operazioni bloccanti.
 
### Gestione dei dispositivi
Tramite VFS vengono esposti diversi parametri che rappresentano lo stato del dispositivo, che possono essere letti o manipolati direttamente dalla CLI.
- **Enable/Disable a device file (8/9)**: Richiede un minor number all’utente e abilita o disabilita il dispositivo associato a quel minor. Per fare ciò scrive il valore 0 o 1 nel file `/sys/module/multiflow_driver/parameters/device_enabling`, nella posizione specifica associata al dispositivo.
- **See device status (10)**: Visualizza tutte le informazioni sullo stato di un dispositivo, specificato dall’utente tramite il minor number. Si accede quindi in lettura ai parametri del modulo `/sys/module/multiflow_driver/parameters/`.

### Altri comandi
La CLI oltre ai comandi descritti presenta altri tre comandi:
- **Create device nodes (11)**:  Genera 128 file nel path di default, o nel path specificato dall’utente tramite secondo argomento. I file generati hanno:
  - Tutti lo stesso major number, indicato tramite il primo argomento dall’utente.
  - Minor numbers progressivi da 0 a 127.
- **Refresh CLI (ENTER o 12)**: Aggiorna le informazioni mostrate nell’header della CLI, utile se più processi hanno sessioni aperte verso lo stesso device. Ad esempio si può visualizzare il nuovo spazio disponibile su un client differente da quello che ha effettuato l’ultima operazione.
- **Exit (-1)**: Chiude il device file attualmente aperto, e termina il programma.