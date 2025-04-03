/*
 * Copyright (C) 2018 dimercur
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "tasks.h"
#include <stdexcept>

#include <iostream>
#include <cstdlib>
#include <unistd.h>

// Déclaration des priorités des taches
#define PRIORITY_TSERVER 30
#define PRIORITY_TOPENCOMROBOT 20
#define PRIORITY_TMOVE 20
#define PRIORITY_TSENDTOMON 22
#define PRIORITY_TRECEIVEFROMMON 25
#define PRIORITY_TSTARTROBOT 20
#define PRIORITY_TCAMERA 19
#define PRIORITY_TWATCHDOG 23

#define WATCHDOG_TESTING            // beaucoup de changement (RISQUÉE)

Camera cam;
Arena arene;

/*
 * Some remarks:
 * 1- This program is mostly a template. It shows you how to create tasks, semaphore
 *   message queues, mutex ... and how to use them
 *
 * 2- semDumber is, as name say, useless. Its goal is only to show you how to use semaphore
 *
 * 3- Data flow is probably not optimal
 *
 * 4- Take into account that ComRobot::Write will block your task when serial buffer is full,
 *   time for internal buffer to flush
 *
 * 5- Same behavior existe for ComMonitor::Write !
 *
 * 6- When you want to write something in terminal, use cout and terminate with endl and flush
 *
 * 7- Good luck !
 */

/**
 * @brief Initialisation des structures de l'application (tâches, mutex,
 * semaphore, etc.)
 */
void Tasks::Init()
{
    int status;
    int err;

    /**************************************************************************************/
    /* 	Mutex creation                                                                    */
    /**************************************************************************************/
    if (err = rt_mutex_create(&mutex_monitor, NULL))
    {
        cerr << "Error mutex create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_mutex_create(&mutex_robot, NULL))
    {
        cerr << "Error mutex create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_mutex_create(&mutex_robotStarted, NULL))
    {
        cerr << "Error mutex create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_mutex_create(&mutex_move, NULL))
    {
        cerr << "Error mutex create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_mutex_create(&mutex_battery, NULL))
    {
        cerr << "Error mutex create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_mutex_create(&mutex_detectionArene, NULL))
    {
        cerr << "Error mutex create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_mutex_create(&mutex_ConfirmationArene, NULL))
    {
        cerr << "Error mutex create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_mutex_create(&mutex_RechercheRobot, NULL))
    {
        cerr << "Error mutex create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_mutex_create(&mutex_demandeRechercheArene, NULL))
    {
        cerr << "Error mutex create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
#ifdef WATCHDOG_TESTING
    if (err = rt_mutex_create(&mutex_DemarageAvecWatchdog, NULL))
    {
        cerr << "Error mutex create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
#endif

    cout << "Mutexes created successfully" << endl
         << flush;

    /**************************************************************************************/
    /* 	Semaphors creation       							  */
    /**************************************************************************************/
    if (err = rt_sem_create(&sem_barrier, NULL, 0, S_FIFO))
    {
        cerr << "Error semaphore create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_sem_create(&sem_openComRobot, NULL, 0, S_FIFO))
    {
        cerr << "Error semaphore create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_sem_create(&sem_serverOk, NULL, 0, S_FIFO))
    {
        cerr << "Error semaphore create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_sem_create(&sem_startRobot, NULL, 0, S_FIFO))
    {
        cerr << "Error semaphore create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_sem_create(&sem_RechercheArene, NULL, 0, S_FIFO))
    {
        cerr << "Error semaphore create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    #ifdef WATCHDOG_TESTING
    if (err = rt_sem_create(&sem_DemarageWatchdog, NULL, 0, S_FIFO))
    {
        cerr << "Error semaphore create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    #endif
    cout << "Semaphores created successfully" << endl << flush;

    /**************************************************************************************/
    /* Tasks creation                                                                     */
    /**************************************************************************************/
    if (err = rt_task_create(&th_server, "th_server", 0, PRIORITY_TSERVER, 0))
    {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_create(&th_sendToMon, "th_sendToMon", 0, PRIORITY_TSENDTOMON, 0))
    {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_create(&th_receiveFromMon, "th_receiveFromMon", 0, PRIORITY_TRECEIVEFROMMON, 0))
    {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_create(&th_openComRobot, "th_openComRobot", 0, PRIORITY_TOPENCOMROBOT, 0))
    {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_create(&th_startRobot, "th_startRobot", 0, PRIORITY_TSTARTROBOT, 0))
    {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_create(&th_move, "th_move", 0, PRIORITY_TMOVE, 0))
    {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_create(&th_camera, "th_camera", 0, PRIORITY_TCAMERA, 0))
    {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_create(&th_recherchearene, "th_recherchearene", 0, PRIORITY_TCAMERA, 0))
    {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    #ifdef WATCHDOG_TESTING
    if (err = rt_task_create(&th_watchdogTask, "th_watchdogTask", 0, PRIORITY_TWATCHDOG, 0))
    {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    #endif

    cout << "Tasks created successfully" << endl << flush;

    /**************************************************************************************/
    /* Message queues creation                                                            */
    /**************************************************************************************/
    if ((err = rt_queue_create(&q_messageToMon, "q_messageToMon", sizeof(Message *) * 50, Q_UNLIMITED, Q_FIFO)) < 0)
    {
        cerr << "Error msg queue create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }    
    if ((err = rt_queue_create(&q_messageToRobot, "q_messageToRobot", sizeof(Message *) * 50, Q_UNLIMITED, Q_FIFO)) < 0)
    {
        cerr << "Error msg queue create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    cout << "Queues created successfully" << endl << flush;
}

/**
 * @brief Démarrage des tâches
 */
void Tasks::Run()
{
    rt_task_set_priority(NULL, T_LOPRIO);
    int err;

    if (err = rt_task_start(&th_server, (void (*)(void *))&Tasks::ServerTask, this))
    {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_sendToMon, (void (*)(void *))&Tasks::SendToMonTask, this))
    {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_receiveFromMon, (void (*)(void *))&Tasks::ReceiveFromMonTask, this))
    {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_openComRobot, (void (*)(void *))&Tasks::OpenComRobot, this))
    {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_startRobot, (void (*)(void *))&Tasks::StartRobotTask, this))
    {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_move, (void (*)(void *))&Tasks::MoveTask, this))
    {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_camera, (void (*)(void *))&Tasks::CameraTask, this))
    {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_recherchearene, (void (*)(void *))&Tasks::RechercheArene, this))
    {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
#ifdef WATCHDOG_TESTING
    if (err = rt_task_start(&th_watchdogTask, (void (*)(void *))&Tasks::Watchdog, this))
    {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
#endif

    cout << "Tasks launched" << endl << flush;
}

/**
 * @brief Arrêt des tâches
 */
void Tasks::Stop()
{
    monitor.Close();
    robot.Close();
}

/**
 */
void Tasks::Join()
{
    cout << "Tasks synchronized" << endl << flush;
    rt_sem_broadcast(&sem_barrier);
    pause();
}

#ifdef WATCHDOG_TESTING
void Tasks::Watchdog(void *arg)
{
    int RobotIsStarted = 0;
    Message *message;

    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;

    rt_sem_p(&sem_barrier, TM_INFINITE);

    rt_sem_p(&sem_DemarageWatchdog, TM_INFINITE);                       // En attente de la libération du sémaphore du démarrage du WatchDog

    rt_task_set_periodic(NULL, TM_NOW, 1000000000);
    
    int failedAttempts = 0;

    while (1)
    {
        rt_task_wait_period(NULL);
        rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
        RobotIsStarted = robotStarted;                                  // Vérification de l'état du robot
        rt_mutex_release(&mutex_robotStarted);
        if (RobotIsStarted == 1)
        {
            rt_mutex_acquire(&mutex_robot, TM_INFINITE);
            message = robot.Write(new Message(MESSAGE_ROBOT_RELOAD_WD));    // Envoie du message de RELOAD
            rt_mutex_release(&mutex_robot);
                        
            if (message == NULL)
            {
                failedAttempts++;
                if (failedAttempts >= 3)                                    // Seuil de 3 échecs consécutifs
                {
                    cout << "Perte de connexion avec le robot !" << endl;
                }
            }
            else
            {
                failedAttempts = 0;
                rt_mutex_acquire(&mutex_robot, TM_INFINITE);
                WriteInQueue(&q_messageToRobot, message);
                rt_mutex_release(&mutex_robot);
            }
        }
    }
    delete (message);
}
#endif

void Tasks::RechercheArene(void *args){
    
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    
    rt_sem_p(&sem_barrier, TM_INFINITE);                                // Sémaphore barrier de lancement des tâches en groupe

    rt_task_set_periodic(NULL, TM_NOW, 100000000);
    
    Img *img;                                                           // Objet IMAGE
    
    while(1){
        
        rt_task_wait_period(NULL);
        
        rt_sem_p(&sem_RechercheArene, TM_INFINITE);                     // Sémaphore bloquant de recherche d'arène
        
        Message *msgSend_INFORMATION;                                   // Message d'information
        MessageImg *msgSend_IMG;                                        // Message d'image
        
        if (cam.IsOpen()){
                        
            rt_mutex_acquire(&mutex_demandeRechercheArene, TM_INFINITE);
            int LOCALdemandedetectionarene = DemandeRechercheArene;     // vrai lorsqu'on demande la detection de l'arene
            rt_mutex_release(&mutex_demandeRechercheArene);

            rt_mutex_acquire(&mutex_ConfirmationArene, TM_INFINITE);
            int LOCALConfirmationArene = ConfirmationArene;             // vrai lorsque l'utilisateur confirme l'arène
            rt_mutex_release(&mutex_ConfirmationArene);
            
            if (LOCALdemandedetectionarene == 1){

                img = new Img(cam.Grab());                              // Capture d'une nouvelle image

                rt_mutex_acquire(&mutex_demandeRechercheArene, TM_INFINITE);
                DemandeRechercheArene = 0;
                rt_mutex_release(&mutex_demandeRechercheArene);

                arene = Arena(img->SearchArena());                      // Tentative de détection d'une arène
                if (arene.IsEmpty() == false)                           // Si une arène à été détectée
                {
                    cout << "    > Arène detectée" << endl << flush;
                    img->DrawArena(arene);                              // Dessin de l'arène sur l'image

                    msgSend_IMG = new MessageImg(MESSAGE_CAM_IMAGE, img);
                    rt_mutex_acquire(&mutex_monitor, TM_INFINITE);
                    WriteInQueue(&q_messageToMon, msgSend_IMG);         // Envoie de l'imge au moniteur
                    rt_mutex_release(&mutex_monitor);

                    rt_mutex_acquire(&mutex_attenteConfirmationArene, TM_INFINITE);
                    AttenteConfirmationArene = 1;
                    rt_mutex_release(&mutex_attenteConfirmationArene);
                }
                else                                                    // Aucune arène trouvée
                {
                    cout << "    > Aucune arène trouvée" << endl << flush;

                    msgSend_INFORMATION = new Message(MESSAGE_ANSWER_NACK);     // Envoie NACK pour aucune arène trouvée
                    rt_mutex_acquire(&mutex_monitor, TM_INFINITE);
                    WriteInQueue(&q_messageToMon, msgSend_INFORMATION);
                    rt_mutex_release(&mutex_monitor);

                    rt_mutex_acquire(&mutex_attenteConfirmationArene, TM_INFINITE);
                    AttenteConfirmationArene = 0;
                    rt_mutex_release(&mutex_attenteConfirmationArene);
                }
            }else if (LOCALConfirmationArene == 1){                             // L'utilisateur à confirmé l'arène
                rt_mutex_acquire(&mutex_detectionArene, TM_INFINITE);
                DetectionArene = 1;                                             // L'arène trouvée
                rt_mutex_release(&mutex_detectionArene);
                rt_mutex_acquire(&mutex_ConfirmationArene, TM_INFINITE);
                ConfirmationArene = -1;
                rt_mutex_release(&mutex_ConfirmationArene);
                rt_mutex_acquire(&mutex_attenteConfirmationArene, TM_INFINITE);
                AttenteConfirmationArene = 0;
                rt_mutex_release(&mutex_attenteConfirmationArene);
                cout << "    > Arène CONFIRMEE PAR L'UTILISATEUR !" << endl << flush;
            }else if (LOCALConfirmationArene == 0){                             // L'utilisateur ne confirme pas l'arène
                rt_mutex_acquire(&mutex_detectionArene, TM_INFINITE);
                DetectionArene = 0;                                             // Arène non trouvée
                rt_mutex_release(&mutex_detectionArene);
                rt_mutex_acquire(&mutex_ConfirmationArene, TM_INFINITE);
                ConfirmationArene = -1;
                rt_mutex_release(&mutex_ConfirmationArene);
                rt_mutex_acquire(&mutex_attenteConfirmationArene, TM_INFINITE);
                AttenteConfirmationArene = 0;
                rt_mutex_release(&mutex_attenteConfirmationArene);

                cout << "    > Arène NON CONFIRMEE PAR L'UTILISATEUR !" << endl << flush;
            }
            
        }else{
            // Message demandant à l'utilisateur d'activer la caméra
            cout << "        > Veilleuez activer d'abord la caméra afin de pouvoir faire une recherche de l'arène" << endl << flush;
        }
        
    }
    
}

void Tasks::CameraTask(void *arg)
{
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    
    rt_sem_p(&sem_barrier, TM_INFINITE);                                // Sémaphore barrier de lancement des tâches en groupe

    rt_task_set_periodic(NULL, TM_NOW, 100000000);

    Img *img;

    while (1)
    {
        MessageImg *msgSend_IMG;                                        // Message d'information
        MessagePosition *msgSend_POSITION;                              // Message de position

        rt_task_wait_period(NULL);

        if (cam.IsOpen())                                               // Vérification de l'ouverture de la caméra
        {
            img = new Img(cam.Grab());                                  // Capture d'une image

            if (img->img.empty())                                       // Si aucune image n'a été prise
            {
                cout << "Image vide " << endl;                          // Prévenir l'utilisateur
            }
            else
            {
                rt_mutex_acquire(&mutex_detectionArene, TM_INFINITE);
                int LOCALdetectionarene = DetectionArene;               // vrai si une arène à été détectée ET confirmée par l'utilisateur
                rt_mutex_release(&mutex_detectionArene);
                
                rt_mutex_acquire(&mutex_attenteConfirmationArene, TM_INFINITE);
                int LOCALAttenteConfirmationArene = AttenteConfirmationArene;   //Lecture de l'attente confirmation de l'arène
                rt_mutex_release(&mutex_attenteConfirmationArene);

                rt_mutex_acquire(&mutex_RechercheRobot, TM_INFINITE);
                int LOCALRechercheRobot = RechercheRobot;               // vrai lorsqu'on demande la recherche du robot
                rt_mutex_release(&mutex_RechercheRobot);

                if (LOCALAttenteConfirmationArene == 0){                // Vérification de l'attente de l'arène
                    
                    if (arene.IsEmpty() == false && LOCALdetectionarene == 1)   // Vérification si l'arène à été validée (et n'est pas vide)
                    {
                        cout << "    > DESSIN ARENE : " << endl << flush;
                        img->DrawArena(arene);                                  // dessin de l'arène sur l'image
                    }
                    
                    if (LOCALRechercheRobot == 1 && LOCALdetectionarene == 1)   // Si la recherche du robot à été activée
                    {
                        cout << "    > RECHERCHE robot en cours" << endl << flush;
                        list<Position> liste_position = img->SearchRobot(arene);    // Sauvegarde de la position
                        Position positionRobot;
                        if (liste_position.empty() == false)                    // Vérification que la position trouvée n'est pas vide (non trouvée)
                        {
                            positionRobot = liste_position.front();             // Si pas vide, ajout position
                            img->DrawRobot(positionRobot);                      // si pas vide, dessin de la flèche sur le robot
                        }
                        else
                        {
                            positionRobot.center = cv::Point2f(-1.0,-1.0);      // Position erreur si robot non détectée
                        }

                        cout << "    > POSITION ROBOT : " << positionRobot.ToString() << endl << flush;

                        msgSend_POSITION = new MessagePosition(MESSAGE_CAM_POSITION, positionRobot);
                        rt_mutex_acquire(&mutex_monitor, TM_INFINITE);
                        WriteInQueue(&q_messageToMon, msgSend_POSITION);        // Envoie de la position au moniteur
                        rt_mutex_release(&mutex_monitor);
                    }
                    
                    msgSend_IMG = new MessageImg(MESSAGE_CAM_IMAGE, img);
                    rt_mutex_acquire(&mutex_monitor, TM_INFINITE);
                    WriteInQueue(&q_messageToMon, msgSend_IMG);                 // Envoie de l'image finale (avec dessin arène et/ou position)
                    rt_mutex_release(&mutex_monitor);
                }
                
                delete img;                                                     // Libération de la mémoire alouée pour l'image
            }
        }
    }
    cout << endl << flush;
}

/**
 * @brief Thread handling server communication with the monitor.
 */
void Tasks::ServerTask(void *arg)
{
    int status;

    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;

    rt_sem_p(&sem_barrier, TM_INFINITE);

    /**************************************************************************************/
    /* The task server starts here                                                        */
    /**************************************************************************************/
    rt_mutex_acquire(&mutex_monitor, TM_INFINITE);
    status = monitor.Open(SERVER_PORT);
    rt_mutex_release(&mutex_monitor);

    cout << "Open server on port " << (SERVER_PORT) << " (" << status << ")" << endl;

    if (status < 0)
        throw std::runtime_error{"Unable to start server on port " + std::to_string(SERVER_PORT)};
    monitor.AcceptClient(); // Wait the monitor client
    cout << "Rock'n'Roll baby, client accepted!" << endl
         << flush;
    rt_sem_broadcast(&sem_serverOk);
}

/**
 * @brief Thread sending data to monitor.
 */
void Tasks::SendToMonTask(void *arg)
{
    Message *msg;

    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);

    /**************************************************************************************/
    /* The task sendToMon starts here                                                     */
    /**************************************************************************************/
    rt_sem_p(&sem_serverOk, TM_INFINITE);

    while (1)
    {
        cout << "wait msg to send" << endl
             << flush;
        msg = ReadInQueue(&q_messageToMon);
        cout << "Send msg to mon: " << msg->ToString() << endl
             << flush;
        rt_mutex_acquire(&mutex_monitor, TM_INFINITE);
        monitor.Write(msg); // The message is deleted with the Write
        rt_mutex_release(&mutex_monitor);
    }
}

void reload_program() {
    const char *program = "./superviseur-robot";                                // Chemin accès programme
    char *const args[] = {const_cast<char*>(program), nullptr};                 // argumennts au démarrage
    
    usleep(1000);                                                               // attente avant reload
    
    execvp(program, args);                                                      // Ré-exécution du programme après arrêt
    
    std::cerr << "Erreur lors du redémarrage du programme." << std::endl;       // Si execvp échoue, on peut afficher un message d'erreur
    exit(-1);
}


/**
 * @brief Thread receiving data from monitor.
 */
void Tasks::ReceiveFromMonTask(void *arg)
{
    Message *msgRcv;

    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);

    /**************************************************************************************/
    /* The task receiveFromMon starts here                                                */
    /**************************************************************************************/
    rt_sem_p(&sem_serverOk, TM_INFINITE);
    cout << "Received message from monitor activated" << endl
         << flush;

    while (1)
    {
        msgRcv = monitor.Read();
        cout << "Rcv <= " << msgRcv->ToString() << endl << flush;

        if (msgRcv->CompareID(MESSAGE_MONITOR_LOST))                            // En cas de perte de connexion avec le moniteur : restart
        {
            delete (msgRcv);

            cout << "Connection lost with monitor, performing cleanup..." << endl << flush;
    
            rt_mutex_acquire(&mutex_robot, TM_INFINITE);
            Message *msgStopRobot = robot.Write(new Message(MESSAGE_ROBOT_STOP));   // Arrêter le robot
            rt_mutex_release(&mutex_robot);
            cout << "Robot stopped." << endl << flush;

            rt_mutex_acquire(&mutex_monitor, TM_INFINITE);
            monitor.Close();                                                    // Fermer le serveur
            rt_mutex_release(&mutex_monitor);
            rt_mutex_acquire(&mutex_robot, TM_INFINITE);
            robot.Close();                                                      // fermer le robot
            rt_mutex_release(&mutex_robot);
            cout << "Server & robot SOCKET closed." << endl << flush;

            cam.Close();  // Ferme la caméra
            cout << "Camera disconnected." << endl << flush;

            // Revenir à l'état initial
            // Réinitialiser des variables ou préparer le système pour un redémarrage
            rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
            robotStarted = 0;                                                   // Réinitialiser l'état du robot
            rt_mutex_release(&mutex_robotStarted);
            cout << "System reset to initial state." << endl << flush;
            
            reload_program();                                                   // Appel fonction de restart du programme
        }
        else if (msgRcv->CompareID(MESSAGE_ROBOT_COM_OPEN))
        {
            rt_sem_v(&sem_openComRobot);
        }
        else if (msgRcv->CompareID(MESSAGE_ROBOT_START_WITHOUT_WD))
        {
            rt_mutex_acquire(&mutex_DemarageAvecWatchdog, TM_INFINITE);
            DemarageAvecWatchdog = 0;
            rt_mutex_release(&mutex_DemarageAvecWatchdog);
            rt_sem_v(&sem_startRobot);
        }

#ifdef WATCHDOG_TESTING
        else if (msgRcv->CompareID(MESSAGE_ROBOT_START_WITH_WD))                // Si démarrage avec WatchDog
        {
            rt_mutex_acquire(&mutex_DemarageAvecWatchdog, TM_INFINITE);
            DemarageAvecWatchdog = 1;                                           // Mise à jour de la variable d'activation du WatchDog
            rt_mutex_release(&mutex_DemarageAvecWatchdog);
            rt_sem_v(&sem_startRobot);                                          // Libération du sémaphore de la tâche du WatchDog
        }
#endif

        else if (msgRcv->CompareID(MESSAGE_ROBOT_GO_FORWARD) ||                 // Si la commande reçus est liée au déplacement du robot
                 msgRcv->CompareID(MESSAGE_ROBOT_GO_BACKWARD) ||
                 msgRcv->CompareID(MESSAGE_ROBOT_GO_RIGHT) ||
                 msgRcv->CompareID(MESSAGE_ROBOT_GO_LEFT) ||
                 msgRcv->CompareID(MESSAGE_ROBOT_STOP))
        {

            rt_mutex_acquire(&mutex_move, TM_INFINITE);
            move = msgRcv->GetID();                                             // Affectation du mouvement
            rt_mutex_release(&mutex_move);
        }
        else if (msgRcv->CompareID(MESSAGE_ROBOT_BATTERY_GET))                  // Si la commande est la demande de la batterie
        {
            rt_mutex_acquire(&mutex_robot, TM_INFINITE);
            MessageBattery *msg;                                                            // Création du message de retour
            msg = (MessageBattery *)robot.Write(new Message(MESSAGE_ROBOT_BATTERY_GET));    // Envoie et récupération du niveau de batterie
            monitor.Write(msg);                                                             // Envoie au moniteur du niveau de batterie retournée par le robot
            rt_mutex_release(&mutex_robot);
        }
        else if (msgRcv->CompareID(MESSAGE_CAM_OPEN))                               // Si la commande est l'ouverture de la caméra
        {
            cam.Open();                                                             // Ouverture de la caméra
            if (cam.IsOpen() == 0)                                                  // Echec ouverture caméra
            { 
                cout << "Problème d'ouverture de la camera" << endl << flush;       // Prévenir l'utilisateur du problème d'ouverture de la caméra

                Message * msgSend_INFORMATION = new Message(MESSAGE_ANSWER_NACK);   // Message NACK
                rt_mutex_acquire(&mutex_monitor, TM_INFINITE);
                WriteInQueue(&q_messageToMon, msgSend_INFORMATION);                 // Envoie du message NACK au moniteur
                rt_mutex_release(&mutex_monitor);
            }
            else                                                                    // Ouverture caméra bueno
            { 
                cout << "Camera ouverte avec succès" << endl << flush;
                Message * msgSend_INFORMATION = new Message(MESSAGE_ANSWER_ACK);    // Message ACK
                rt_mutex_acquire(&mutex_monitor, TM_INFINITE);
                WriteInQueue(&q_messageToMon, msgSend_INFORMATION);                 // Envoie du message ACK au moniteur
                rt_mutex_release(&mutex_monitor);
            }
        }
        else if (msgRcv->CompareID(MESSAGE_CAM_CLOSE))                              // Si le message est la demande de fermeture de la caméra
        {
            cam.Close();                                                            // Fermeture de la caméra
        }
        else if (msgRcv->CompareID(MESSAGE_CAM_ASK_ARENA))                          // Si la demande de l'arène est activée
        {
            rt_mutex_acquire(&mutex_demandeRechercheArene, TM_INFINITE);
            DemandeRechercheArene = 1;                                          // Mise à jour de la variable de demande de recherche de l'arène
            rt_mutex_release(&mutex_demandeRechercheArene);
            rt_sem_v(&sem_RechercheArene);                                      // Libération du sémaphore pour débloquer la tâche de recherche d'arène
        }
        else if (msgRcv->CompareID(MESSAGE_CAM_ARENA_CONFIRM))                  // L'utilisateur confirme l'arène
        {
            rt_mutex_acquire(&mutex_ConfirmationArene, TM_INFINITE);
            ConfirmationArene = 1;                                              // Mise à jour de la variable de réponse "OUI" de confirmation d'arène
            rt_mutex_release(&mutex_ConfirmationArene);
            rt_sem_v(&sem_RechercheArene);                                      // Libération du sémaphore pour débloquer la tâche de confirmation d'arène
        }
        else if (msgRcv->CompareID(MESSAGE_CAM_ARENA_INFIRM))                   // L'utilisateur ne confirme PAS l'arène
        {
            rt_mutex_acquire(&mutex_ConfirmationArene, TM_INFINITE);
            ConfirmationArene = 0;                                              // Mise à jour de la variable de réponse "NON" de confirmation d'arène
            rt_mutex_release(&mutex_ConfirmationArene);
            rt_sem_v(&sem_RechercheArene);                                      // Libération du sémaphore pour débloquer la tâche de recherche d'arène
        }
        else if (msgRcv->CompareID(MESSAGE_CAM_POSITION_COMPUTE_START))         // Demande de l'activation du calcul de la position du robot
        {
            rt_mutex_acquire(&mutex_RechercheRobot, TM_INFINITE);
            RechercheRobot = 1;                                                 // Mise à jour de la variable de recherche du robot (ON)
            rt_mutex_release(&mutex_RechercheRobot);
        }
        else if (msgRcv->CompareID(MESSAGE_CAM_POSITION_COMPUTE_STOP))          // Demande de l'arrêt du calcul de la position du robot
        {
            rt_mutex_acquire(&mutex_RechercheRobot, TM_INFINITE);
            RechercheRobot = 0;                                                 // Mise à jour de la variable de recherche du robot (OFF)
            rt_mutex_release(&mutex_RechercheRobot);
        }
        delete (msgRcv);
    }
}

/**
 * @brief Thread opening communication with the robot.
 */
void Tasks::OpenComRobot(void *arg)
{
    int status;
    int err;

    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);

    /**************************************************************************************/
    /* The task openComRobot starts here                                                  */
    /**************************************************************************************/
    while (1)
    {
        rt_sem_p(&sem_openComRobot, TM_INFINITE);
        cout << "Open serial com (";
        rt_mutex_acquire(&mutex_robot, TM_INFINITE);
        status = robot.Open();
        rt_mutex_release(&mutex_robot);
        cout << status;
        cout << ")" << endl
             << flush;

        Message *msgSend;
        if (status < 0)
        {
            msgSend = new Message(MESSAGE_ANSWER_NACK);
        }
        else
        {
            msgSend = new Message(MESSAGE_ANSWER_ACK);
        }
        WriteInQueue(&q_messageToMon, msgSend); // msgSend will be deleted by sendToMon
    }
}

/**
 * @brief Thread starting the communication with the robot.
 */
void Tasks::StartRobotTask(void *arg)
{
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);

#ifdef WATCHDOG_TESTING
    int LOCAL_DemarageAvecWatchdog = 0;
#endif

    /**************************************************************************************/
    /* The task startRobot starts here                                                    */
    /**************************************************************************************/
    while (1)
    {

#ifdef WATCHDOG_TESTING

        Message *msgSend;
        rt_sem_p(&sem_startRobot, TM_INFINITE);

        rt_mutex_acquire(&mutex_DemarageAvecWatchdog, TM_INFINITE);
        LOCAL_DemarageAvecWatchdog = DemarageAvecWatchdog;                      // Vérification de l'état du WatchDog
        rt_mutex_release(&mutex_DemarageAvecWatchdog);

        if (LOCAL_DemarageAvecWatchdog == 0)                                    // Si WatchDog désactivé
        {
            cout << "Start robot without watchdog (";
            rt_mutex_acquire(&mutex_robot, TM_INFINITE);
            msgSend = robot.Write(robot.StartWithoutWD());                      // Envoie du message SANS WD
            rt_mutex_release(&mutex_robot);
            cout << msgSend->GetID();
            cout << ")" << endl;
        }
        else if (LOCAL_DemarageAvecWatchdog == 1)                               // Si message activé
        {
            cout << "Start robot with watchdog (";
            rt_mutex_acquire(&mutex_robot, TM_INFINITE);
            msgSend = robot.Write(robot.StartWithWD());                         // envoie du message AVEC WD
            rt_mutex_release(&mutex_robot);
            cout << msgSend->GetID();
            cout << ")" << endl;
            rt_sem_v(&sem_DemarageWatchdog);
        }

        cout << "Movement answer: " << msgSend->ToString() << endl << flush;
        WriteInQueue(&q_messageToMon, msgSend);

        if (msgSend->GetID() == MESSAGE_ANSWER_ACK)                             // Si acquittement du moniteur
        {
            rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
            robotStarted = 1;
            rt_mutex_release(&mutex_robotStarted);
        }

#else
        Message *msgSend;
        rt_sem_p(&sem_startRobot, TM_INFINITE);
        cout << "Start robot without watchdog (";
        rt_mutex_acquire(&mutex_robot, TM_INFINITE);
        msgSend = robot.Write(robot.StartWithoutWD());
        rt_mutex_release(&mutex_robot);
        cout << msgSend->GetID();
        cout << ")" << endl;

        cout << "Movement answer: " << msgSend->ToString() << endl
             << flush;
        WriteInQueue(&q_messageToMon, msgSend); // msgSend will be deleted by sendToMon

        if (msgSend->GetID() == MESSAGE_ANSWER_ACK)
        {
            rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
            robotStarted = 1;
            rt_mutex_release(&mutex_robotStarted);
        }

#endif
    }
}

/**
 * @brief Thread handling control of the robot.
 */
void Tasks::MoveTask(void *arg)
{
    int rs;
    int cpMove;

    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);

    /**************************************************************************************/
    /* The task starts here                                                               */
    /**************************************************************************************/
    rt_task_set_periodic(NULL, TM_NOW, 100000000);

    while (1)
    {
        rt_task_wait_period(NULL);
        cout << "Periodic movement update";
        rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
        rs = robotStarted;
        rt_mutex_release(&mutex_robotStarted);
        if (rs == 1)
        {
            rt_mutex_acquire(&mutex_move, TM_INFINITE);
            cpMove = move;
            rt_mutex_release(&mutex_move);

            cout << " move: " << cpMove;

            rt_mutex_acquire(&mutex_robot, TM_INFINITE);
            robot.Write(new Message((MessageID)cpMove));
            rt_mutex_release(&mutex_robot);
        }
        cout << endl << flush;
    }
}

/**
 * Write a message in a given queue
 * @param queue Queue identifier
 * @param msg Message to be stored
 */
void Tasks::WriteInQueue(RT_QUEUE *queue, Message *msg)
{
    int err;
    if ((err = rt_queue_write(queue, (const void *)&msg, sizeof((const void *)&msg), Q_NORMAL)) < 0)
    {
        cerr << "Write in queue failed: " << strerror(-err) << endl << flush;
        throw std::runtime_error{"Error in write in queue"};
    }
}

/**
 * Read a message from a given queue, block if empty
 * @param queue Queue identifier
 * @return Message read
 */
Message *Tasks::ReadInQueue(RT_QUEUE *queue)
{
    int err;
    Message *msg;

    if ((err = rt_queue_read(queue, &msg, sizeof((void *)&msg), TM_INFINITE)) < 0)
    {
        cout << "Read in queue failed: " << strerror(-err) << endl << flush;
        throw std::runtime_error{"Error in read in queue"};
    } /** else {
         cout << "@msg :" << msg << endl << flush;
     } /**/

    return msg;
}
