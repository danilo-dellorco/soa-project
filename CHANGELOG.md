# Multiflow Device Driver Changelog 

## Secondo Ricevimento v1.1

  - **Modificato `kfree(session) -> kfree(filp->private_data)` nella `dev_release`**
    - Quando viene viene chiuso un file si va a deallocare solo la struttura `session_state` che mantiene i metadati della singola sessione del processo verso quel file.
    - Processi diversi sullo stesso file avranno due `session_state` differenti, quindi se un processo chiude il file, solo la sua struttura viene deallocata.
  
  - **Aggiornamento atomico sullo spazio libero nel dispositivo**
    - La funzione `write_on_stream` non si occupa ora di ottenere il lock
    - Il lock si ottiene nella dev_write tramite `mutex_trylock`
    - Dopo aver ottenuto il lock si controllano il numero di byte disponibili. Se c'è spazio sufficiente si procede con la scrittura
  - **Scritture HIGH_PRIORITY**
    1. Si scrive sul flusso con la `write_on_stream`.
    2. Si aggiornano le informazioni sul numero di bytes disponibili in base al risultato della write.
    3. Si rilascia il lock.
  - **Scritture LOW_PRIORITY**
    2. Si aggiorna in modo sincrono il numero di bytes disponibili, allocando logicamente lo spazio necessario.
    3. Si rilascia il lock.
    4. Quando viene schedulata la `write_deferred`, il demone attende attivamente il lock tramite `mutex_lock`.
    5. Dopo aver ottenuto il lock si scrive sul flusso dati. La scrittura non può fallire.
    6. Si rilascia il lock.

  - **La scrittura low_priority non può fallire**
    - Dopo aver ottenuto il lock si alloca la `packed_work` ed il buffer temporaneo `packed_work->data`.
    - Se queste operazioni falliscono viene notificato in maniera sincrona all'utente l'errore in scrittura.
    - La `deferred_write` viene schedulata solo se tutte le strutture necessarie alla scrittura vengono allocate correttamente.

  - **Cambiata api di locking da `try_mutex_lock` in `get_lock`**
    - Il parametro `lock_type` specifica se è necessario usare `mutex_lock` o `mutex_trylock`.
    - Le scritture low priority specificano `LOCK` per attendere il lock.
    - Tutte le altre operazioni sincrone specificano `TRYLOCK`. Se il lock non è disponibile si vede se l'operazione è bloccante o non-bloccante.
  
  - **Separata `write_on_stream` e `write_deferred`.**
    - Le scritture high e low vengono fatte ora in due api separate.
    - Tutto lo spazio necessario alla scrittura deferred viene riservato in maniera atomica prima di notificare il risultato. 
    - Se la memoria è sufficiente, una volta schedulata la scrittura non potrà più fallire.

  - Fixata validazione input `user_cli`
  - Spostata logica di release del lock in `release_lock`

## Terzo Ricevimento v1.2
  - **La `copy_from_user` nella `schedule_write` ora non ritorna errore se vengono copiati solo una parte dei bytes totale.**
  - Rimosso comando di test
  - Sistemata `printk` in `get_lock`