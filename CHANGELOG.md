# Multiflow Device Driver Changelog 

## Secondo Ricevimento v1.1

#### 14-09-2022
- **Modificato `kfree(session) -> kfree(filp->private_data)` nella `dev_release`**
  - Quando viene viene chiuso un file si va a deallocare solo la struttura `session_state` che mantiene i metadati della singola sessione del processo verso quel file.
  - Processi diversi sullo stesso file avranno due `session_state` differenti, quindi se un processo chiude il file, solo la sua struttura viene deallocata.

- **Aggiornamento atomico sullo spazio libero nel dispositivo**
  - La funzione write_on_stream non si occupa ora di ottenere il lock
  - Il lock si ottiene nella dev_write
  - Dopo aver ottenuto il lock si controllano il numero di byte disponibili
  - **Scritture HIGH_PRIORITY**
    1. Si ottiene il lock tramite `mutex_trylock` e si scrive sul flusso con la `write_on_stream`
    2. Si aggiornano le informazioni sul numero di bytes disponibili
    3. Si rilascia il lock
  - **Scritture LOW_PRIORITY**
    1. Si ottiene il lock tramite `mutex_trylock` e si schedula la scrittura con la `schedule_write`
    2. Si aggiorna in modo sincrono il numero di bytes disponibili
    3. Si rilascia il lock
    4. Quando viene schedulata la `write_deferred` si ottiene il lock per la scrittura tramite `mutex_lock`
    5. Dopo aver ottenuto il lock si scrive sul flusso con la `write_on_stream`, che non può fallire.
    6. 

- **Migliorato cleanup del modulo**
  - Rimosse kfree errate
  - Deallocazione dei blocchi stream rimanenti

- **Fixata validazione input user_cli**