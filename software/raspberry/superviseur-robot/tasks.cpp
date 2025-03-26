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

// Déclaration des priorités des taches
#define PRIORITY_TSERVER 30
#define PRIORITY_TOPENCOMROBOT 20
#define PRIORITY_TMOVE 20
#define PRIORITY_TSENDTOMON 22
#define PRIORITY_TRECEIVEFROMMON 25
#define PRIORITY_TSTARTROBOT 20
#define PRIORITY_TCAMERA 19
#define PRIORITY_TWATCHDOG 23

#define WATCHDOG_TESTING

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
        cerr << "Error mutex create: " << strerror(-err) << endl
             << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_mutex_create(&mutex_robot, NULL))
    {
        cerr << "Error mutex create: " << strerror(-err) << endl
             << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_mutex_create(&mutex_robotStarted, NULL))
    {
        cerr << "Error mutex create: " << strerror(-err) << endl
             << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_mutex_create(&mutex_move, NULL))
    {
        cerr << "Error mutex create: " << strerror(-err) << endl
             << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_mutex_create(&mutex_battery, NULL))
    {
        cerr << "Error mutex create: " << strerror(-err) << endl
             << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_mutex_create(&mutex_detectionArene, NULL))
    {
        cerr << "Error mutex create: " << strerror(-err) << endl
             << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_mutex_create(&mutex_ConfirmationArene, NULL))
    {
        cerr << "Error mutex create: " << strerror(-err) << endl
             << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_mutex_create(&mutex_RechercheRobot, NULL))
    {
        cerr << "Error mutex create: " << strerror(-err) << endl
             << flush;
        exit(EXIT_FAILURE);
    }
#ifdef WATCHDOG_TESTING
    if (err = rt_mutex_create(&mutex_DemarageAvecWatchdog, NULL))
    {
        cerr << "Error mutex create: " << strerror(-err) << endl
             << flush;
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
        cerr << "Error semaphore create: " << strerror(-err) << endl
             << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_sem_create(&sem_openComRobot, NULL, 0, S_FIFO))
    {
        cerr << "Error semaphore create: " << strerror(-err) << endl
             << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_sem_create(&sem_serverOk, NULL, 0, S_FIFO))
    {
        cerr << "Error semaphore create: " << strerror(-err) << endl
             << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_sem_create(&sem_startRobot, NULL, 0, S_FIFO))
    {
        cerr << "Error semaphore create: " << strerror(-err) << endl
             << flush;
        exit(EXIT_FAILURE);
    }
    #ifdef WATCHDOG_TESTING
    if (err = rt_sem_create(&sem_DemarageWatchdog, NULL, 0, S_FIFO))
    {
        cerr << "Error semaphore create: " << strerror(-err) << endl
             << flush;
        exit(EXIT_FAILURE);
    }
    #endif
    cout << "Semaphores created successfully" << endl
         << flush;

    /**************************************************************************************/
    /* Tasks creation                                                                     */
    /**************************************************************************************/
    if (err = rt_task_create(&th_server, "th_server", 0, PRIORITY_TSERVER, 0))
    {
        cerr << "Error task create: " << strerror(-err) << endl
             << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_create(&th_sendToMon, "th_sendToMon", 0, PRIORITY_TSENDTOMON, 0))
    {
        cerr << "Error task create: " << strerror(-err) << endl
             << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_create(&th_receiveFromMon, "th_receiveFromMon", 0, PRIORITY_TRECEIVEFROMMON, 0))
    {
        cerr << "Error task create: " << strerror(-err) << endl
             << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_create(&th_openComRobot, "th_openComRobot", 0, PRIORITY_TOPENCOMROBOT, 0))
    {
        cerr << "Error task create: " << strerror(-err) << endl
             << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_create(&th_startRobot, "th_startRobot", 0, PRIORITY_TSTARTROBOT, 0))
    {
        cerr << "Error task create: " << strerror(-err) << endl
             << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_create(&th_move, "th_move", 0, PRIORITY_TMOVE, 0))
    {
        cerr << "Error task create: " << strerror(-err) << endl
             << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_create(&th_camera, "th_camera", 0, PRIORITY_TCAMERA, 0))
    {
        cerr << "Error task create: " << strerror(-err) << endl
             << flush;
        exit(EXIT_FAILURE);
    }
    #ifdef WATCHDOG_TESTING
    if (err = rt_task_create(&th_watchdogTask, "th_watchdogTask", 0, PRIORITY_TWATCHDOG, 0))
    {
        cerr << "Error task create: " << strerror(-err) << endl
             << flush;
        exit(EXIT_FAILURE);
    }
    #endif

    cout << "Tasks created successfully" << endl
         << flush;

    /**************************************************************************************/
    /* Message queues creation                                                            */
    /**************************************************************************************/
    if ((err = rt_queue_create(&q_messageToMon, "q_messageToMon", sizeof(Message *) * 50, Q_UNLIMITED, Q_FIFO)) < 0)
    {
        cerr << "Error msg queue create: " << strerror(-err) << endl
             << flush;
        exit(EXIT_FAILURE);
    }
    cout << "Queues created successfully" << endl
         << flush;
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
        cerr << "Error task start: " << strerror(-err) << endl
             << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_sendToMon, (void (*)(void *))&Tasks::SendToMonTask, this))
    {
        cerr << "Error task start: " << strerror(-err) << endl
             << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_receiveFromMon, (void (*)(void *))&Tasks::ReceiveFromMonTask, this))
    {
        cerr << "Error task start: " << strerror(-err) << endl
             << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_openComRobot, (void (*)(void *))&Tasks::OpenComRobot, this))
    {
        cerr << "Error task start: " << strerror(-err) << endl
             << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_startRobot, (void (*)(void *))&Tasks::StartRobotTask, this))
    {
        cerr << "Error task start: " << strerror(-err) << endl
             << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_move, (void (*)(void *))&Tasks::MoveTask, this))
    {
        cerr << "Error task start: " << strerror(-err) << endl
             << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_camera, (void (*)(void *))&Tasks::CameraTask, this))
    {
        cerr << "Error task start: " << strerror(-err) << endl
             << flush;
        exit(EXIT_FAILURE);
    }
#ifdef WATCHDOG_TESTING
    if (err = rt_task_start(&th_watchdogTask, (void (*)(void *))&Tasks::Watchdog, this))
    {
        cerr << "Error task start: " << strerror(-err) << endl
             << flush;
        exit(EXIT_FAILURE);
    }
#endif

    cout << "Tasks launched" << endl
         << flush;
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
    cout << "Tasks synchronized" << endl
         << flush;
    rt_sem_broadcast(&sem_barrier);
    pause();
}

#ifdef WATCHDOG_TESTING
void Tasks::Watchdog(void *arg)
{
    int rs = 0;
    Message *message;

    cout << "Start " << __PRETTY_FUNCTION__ << endl
         << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);

    rt_sem_p(&sem_DemarageWatchdog, TM_INFINITE);

    rt_task_set_periodic(NULL, TM_NOW, 1000000000); // T=1s

    while (1)
    {
        rt_task_wait_period(NULL);
        rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
        rs = robotStarted;
        rt_mutex_release(&mutex_robotStarted);
        if (rs == 1)
        {
            rt_mutex_acquire(&mutex_robot, TM_INFINITE);
            message = robot.Write(new Message(MESSAGE_ROBOT_RELOAD_WD));
            rt_mutex_release(&mutex_robot);
            WriteInQueue(q_messageToRobot, message); // to verify if we lost connectivity
        }
    }
    delete (message);
}
#endif

void Tasks::CameraTask(void *arg)
{
    cout << "Start " << __PRETTY_FUNCTION__ << endl
         << flush;
    rt_sem_p(&sem_barrier, TM_INFINITE);

    rt_task_set_periodic(NULL, TM_NOW, 100000000);

    Img *img;

    while (1)
    {
        cout << "camera coucou" << endl
             << flush;
        MessageImg *msgSend;
        rt_task_wait_period(NULL);

        if (cam.IsOpen())
        {
            cout << "Periodic Camera update" << endl;

            img = new Img(cam.Grab());

            // cout << img->img.size << endl;
            if (img->img.empty())
            {
                cout << "Image vide " << endl;
            }
            else
            {

                rt_mutex_acquire(&mutex_detectionArene, TM_INFINITE);
                int LOCALdetectionarene = DetectionArene; // vrai lorsqu'on demande la detection de l'arene
                DetectionArene = 0;
                rt_mutex_release(&mutex_detectionArene);

                rt_mutex_acquire(&mutex_ConfirmationArene, TM_INFINITE);
                int LOCALConfirmationArene = ConfirmationArene; // vrai lorsqu'on confirme l'arene
                rt_mutex_release(&mutex_ConfirmationArene);

                rt_mutex_acquire(&mutex_RechercheRobot, TM_INFINITE);
                int LOCALRechercheRobot = RechercheRobot; // vrai lorsqu'on demande la recherche du robot
                rt_mutex_release(&mutex_RechercheRobot);

                if (LOCALRechercheRobot == 1)
                {
                    cout << "recherche robot en cours" << endl
                         << flush;
                    list<Position> liste_position = img->SearchRobot(arene);
                    if (liste_position.empty() == false)
                    {
                        img->DrawRobot(liste_position.front());
                    }
                }

                if (LOCALdetectionarene == 1 && LOCALConfirmationArene == 0)
                {
                    arene = Arena(img->SearchArena());
                    if (arene.IsEmpty() == false)
                    {
                        cout << "camera pas vide" << endl
                             << flush;
                        img->DrawArena(arene);
                        attente_de_confirmation = 1;
                    }
                }
                else if (LOCALConfirmationArene == 1)
                {
                    attente_de_confirmation = 0;
                    img->DrawArena(arene);
                }
                else if (attente_de_confirmation == 1)
                {
                    img->DrawArena(arene);
                }

                msgSend = new MessageImg(MESSAGE_CAM_IMAGE, img);
                rt_mutex_acquire(&mutex_monitor, TM_INFINITE);
                WriteInQueue(&q_messageToMon, msgSend);
                rt_mutex_release(&mutex_monitor);

                delete img;
            }
        }
    }
    cout << endl
         << flush;
}

/**
 * @brief Thread handling server communication with the monitor.
 */
void Tasks::ServerTask(void *arg)
{
    int status;

    cout << "Start " << __PRETTY_FUNCTION__ << endl
         << flush;
    // Synchronization barrier (waiting that all tasks are started)
    rt_sem_p(&sem_barrier, TM_INFINITE);

    /**************************************************************************************/
    /* The task server starts here                                                        */
    /**************************************************************************************/
    rt_mutex_acquire(&mutex_monitor, TM_INFINITE);
    status = monitor.Open(SERVER_PORT);
    rt_mutex_release(&mutex_monitor);

    cout << "Open server on port " << (SERVER_PORT) << " (" << status << ")" << endl;

    if (status < 0)
        throw std::runtime_error{
            "Unable to start server on port " + std::to_string(SERVER_PORT)};
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

    cout << "Start " << __PRETTY_FUNCTION__ << endl
         << flush;
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

/**
 * @brief Thread receiving data from monitor.
 */
void Tasks::ReceiveFromMonTask(void *arg)
{
    Message *msgRcv;

    cout << "Start " << __PRETTY_FUNCTION__ << endl
         << flush;
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
        cout << "Rcv <= " << msgRcv->ToString() << endl
             << flush;

        if (msgRcv->CompareID(MESSAGE_MONITOR_LOST))
        {
            delete (msgRcv);
            exit(-1);
        }
        else if (msgRcv->CompareID(MESSAGE_ROBOT_COM_OPEN))
        {
            rt_sem_v(&sem_openComRobot);
        }
        else if (msgRcv->CompareID(MESSAGE_ROBOT_START_WITHOUT_WD))
        {
            rt_sem_v(&sem_startRobot);
        }

#ifdef WATCHDOG_TESTING
        // start with WD
        else if (msgRcv->CompareID(MESSAGE_ROBOT_START_WITH_WD))
        {
            rt_mutex_acquire(&mutex_DemarageAvecWatchdog, TM_INFINITE);
            DemarageAvecWatchdog = 1;
            rt_mutex_release(&mutex_DemarageAvecWatchdog);
            rt_sem_v(&sem_startRobot);
        }
#endif

        else if (msgRcv->CompareID(MESSAGE_ROBOT_GO_FORWARD) ||
                 msgRcv->CompareID(MESSAGE_ROBOT_GO_BACKWARD) ||
                 msgRcv->CompareID(MESSAGE_ROBOT_GO_RIGHT) ||
                 msgRcv->CompareID(MESSAGE_ROBOT_STOP))
        {

            rt_mutex_acquire(&mutex_move, TM_INFINITE);
            move = msgRcv->GetID();
            rt_mutex_release(&mutex_move);
        }
        else if (msgRcv->CompareID(MESSAGE_ROBOT_BATTERY_GET))
        {
            rt_mutex_acquire(&mutex_robot, TM_INFINITE);
            MessageBattery *msg;
            msg = (MessageBattery *)robot.Write(new Message(MESSAGE_ROBOT_BATTERY_GET));
            monitor.Write(msg);
            rt_mutex_release(&mutex_robot);
        }
        else if (msgRcv->CompareID(MESSAGE_CAM_OPEN))
        {

            cam.Open();
        }
        else if (msgRcv->CompareID(MESSAGE_CAM_CLOSE))
        {

            cam.Close();
        }
        else if (msgRcv->CompareID(MESSAGE_CAM_ASK_ARENA))
        {
            rt_mutex_acquire(&mutex_detectionArene, TM_INFINITE);
            DetectionArene = 1;
            rt_mutex_release(&mutex_detectionArene);
        }
        else if (msgRcv->CompareID(MESSAGE_CAM_ARENA_CONFIRM))
        {
            rt_mutex_acquire(&mutex_ConfirmationArene, TM_INFINITE);
            ConfirmationArene = 1;
            rt_mutex_release(&mutex_ConfirmationArene);
        }
        else if (msgRcv->CompareID(MESSAGE_CAM_ARENA_INFIRM))
        {
            rt_mutex_acquire(&mutex_ConfirmationArene, TM_INFINITE);
            ConfirmationArene = 0;
            rt_mutex_release(&mutex_ConfirmationArene);
        }
        else if (msgRcv->CompareID(MESSAGE_CAM_POSITION_COMPUTE_START))
        {
            rt_mutex_acquire(&mutex_RechercheRobot, TM_INFINITE);
            RechercheRobot = 1;
            rt_mutex_release(&mutex_RechercheRobot);
        }
        else if (msgRcv->CompareID(MESSAGE_CAM_POSITION_COMPUTE_STOP))
        {
            rt_mutex_acquire(&mutex_RechercheRobot, TM_INFINITE);
            RechercheRobot = 0;
            rt_mutex_release(&mutex_RechercheRobot);
        }
        delete (msgRcv); // mus be deleted manually, no consumer
    }
}

/**
 * @brief Thread opening communication with the robot.
 */
void Tasks::OpenComRobot(void *arg)
{
    int status;
    int err;

    cout << "Start " << __PRETTY_FUNCTION__ << endl
         << flush;
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
    cout << "Start " << __PRETTY_FUNCTION__ << endl
         << flush;
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
        LOCAL_DemarageAvecWatchdog = DemarageAvecWatchdog;
        rt_mutex_release(&mutex_DemarageAvecWatchdog);

        if (LOCAL_DemarageAvecWatchdog == 0)
        {
            cout << "Start robot without watchdog (";
            rt_mutex_acquire(&mutex_robot, TM_INFINITE);
            msgSend = robot.Write(robot.StartWithoutWD());
            rt_mutex_release(&mutex_robot);
            cout << msgSend->GetID();
            cout << ")" << endl;
        }
        else if (LOCAL_DemarageAvecWatchdog == 1)
        {
            cout << "Start robot with watchdog (";
            rt_mutex_acquire(&mutex_robot, TM_INFINITE);
            msgSend = robot.Write(robot.StartWithWD());
            rt_mutex_release(&mutex_robot);
            cout << msgSend->GetID();
            cout << ")" << endl;
            rt_sem_v(&sem_DemarageWatchdog);
        }

        cout << "Movement answer: " << msgSend->ToString() << endl
             << flush;
        WriteInQueue(&q_messageToMon, msgSend); // msgSend will be deleted by sendToMon

        if (msgSend->GetID() == MESSAGE_ANSWER_ACK)
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

    cout << "Start " << __PRETTY_FUNCTION__ << endl
         << flush;
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
        cout << endl
             << flush;
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
        cerr << "Write in queue failed: " << strerror(-err) << endl
             << flush;
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
        cout << "Read in queue failed: " << strerror(-err) << endl
             << flush;
        throw std::runtime_error{"Error in read in queue"};
    } /** else {
         cout << "@msg :" << msg << endl << flush;
     } /**/

    return msg;
}
