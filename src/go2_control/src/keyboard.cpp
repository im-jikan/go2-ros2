#include <rclcpp/rclcpp.hpp>                      // ROS2 C++ client library principale
#include <geometry_msgs/msg/twist.hpp>            // Type de message Twist pour commandes de vélocité
#include <termios.h>                              // Pour gérer le mode terminal POSIX
#include <unistd.h>                               // Pour fonctions POSIX (read, usleep)
#include <fcntl.h>                                // Pour flags (F_GETFL, F_SETFL)
#include <iostream>                              // IO standard (cout)
#include <csignal>                               // Gestion des signaux (CTRL+C)


class KeyboardControlNode : public rclcpp::Node
{
public:
    KeyboardControlNode() : rclcpp::Node("keyboard_azerty_teleop")
    {
        // Création du publisher pour envoyer les commandes de vitesse (Twist) au robot
        move_publisher_ = this->create_publisher<geometry_msgs::msg::Twist>("/go2/move", 10);
        
        // Log d'information que le node est prêt à recevoir des commandes clavier
        RCLCPP_INFO(this->get_logger(), "Contrôle clavier (AZERTY) prêt. Utilisez z/s/q/d pour bouger.");
    }


    void run()
    {
        set_terminal_mode();              // Mettre le terminal en mode raw (non-bloquant et sans echo)
        instance_ = this;                 // Référence statique pour gestion signal

        char key;                        // Variable pour stocker la touche lue
        while (rclcpp::ok())             // Tant que ROS n'est pas arrêté
        {
            geometry_msgs::msg::Twist cmd;   // Message Twist pour la commande à publier
            bool publish = false;             // Indicateur si on doit publier un cmd

            if (read_key(key))                // Lecture non-bloquante d'une touche
            {
                // Analyse de la touche pressée et mappage vers commandes
                switch (key)
                {
                    case 'z': cmd.linear.x = 1.0; publish = true; break;   // Avancer
                    case 's': cmd.linear.x = -1.0; publish = true; break;  // Reculer
                    case 'q': cmd.linear.y = 1.0; publish = true; break;   // Gauche (translation latérale)
                    case 'd': cmd.linear.y = -1.0; publish = true; break;  // Droite (translation latérale)
                    case 'a': cmd.linear.z = 1.0; publish = true; break;  // Rotation gauche
                    case 'e': cmd.linear.z = -1.0; publish = true; break; // Rotation droite
                    case ' ':                                                   
                        RCLCPP_INFO(this->get_logger(), "Stop");                 // Stop => Twist nul
                        publish = true;                                         
                        break;

                    // Commandes système spécifiques liées à d'autres nœuds ros2
                    case 'l':
                        RCLCPP_INFO(this->get_logger(), "Lock !");
                        system("ros2 run go2_basics lock");                     // Verrouiller robot
                        break;
                    case 'u':
                        RCLCPP_INFO(this->get_logger(), "Unlock !");
                        system("ros2 run go2_basics unlock");                   // Déverrouiller robot
                        break;
                    case 'h':
                        RCLCPP_INFO(this->get_logger(), "Hello !");
                        system("ros2 run go2_basics hello");                    // Faire dire "Hello"
                        break;
                    case 'j':
                        RCLCPP_INFO(this->get_logger(), "Jump !");
                        system("ros2 run go2_basics jump");                     // Sauter
                        break;
                    case 'p':
                        RCLCPP_INFO(this->get_logger(), "Pounce !");
                        system("ros2 run go2_basics pounce");                   // Sauter sur cible
                        break;
                    case 'c':
                        RCLCPP_INFO(this->get_logger(), "Scrape !");
                        system("ros2 run go2_basics scrape");                   // Gratter
                        break;
                    case 'k':
                        RCLCPP_INFO(this->get_logger(), "Stand Up !");
                        system("ros2 run go2_basics stand_up");                 // Se lever
                        break;
                    case 'm':
                        RCLCPP_INFO(this->get_logger(), "Stand Down !");
                        system("ros2 run go2_basics stand_down");               // Se coucher
                        break;

                    case 27: // ESC - demander la sortie de l’application
                        RCLCPP_INFO(this->get_logger(), "Fermeture demandée (ESC).");
                        reset_terminal_mode();                                  // Restaurer le terminal
                        instance_ = nullptr;                                    // Nettoyage référence statique
                        return;
                    default: // Toute autre touche est ignorée
                        break;
                }
                if (publish) {
                    // Publier la commande Twist uniquement si on a une commande valide
                    move_publisher_->publish(cmd);
                }
            }
            // Permet à ROS de traiter notamment les callbacks, timers éventuels
            rclcpp::spin_some(shared_from_this());

            usleep(50000); // Pause pour limiter la fréquence (20 Hz)
        }

        reset_terminal_mode(); // Après sortie boucle, restaurer terminal
        instance_ = nullptr; 
    }


    void set_terminal_mode()
    {
        tcgetattr(STDIN_FILENO, &orig_termios_); // Sauvegarder configuration initiale
        struct termios newt = orig_termios_;
        newt.c_lflag &= ~(ICANON | ECHO);        // Désactiver mode canonique et echo
        tcsetattr(STDIN_FILENO, TCSANOW, &newt); // Appliquer la nouvelle configuration

        // Rendre la lecture sur stdin non bloquante
        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    }


    void reset_terminal_mode()
    {
        tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios_);
    }


    bool read_key(char &c)
    {
        int n = read(STDIN_FILENO, &c, 1);
        return (n == 1);
    }

    // Pointeur statique pour gestion sécurisée du terminal sur interruption
    static KeyboardControlNode * instance_;

private:
    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr move_publisher_; // Publisher commandes vitesse
    struct termios orig_termios_; // Configuration initiale terminal sauvegardée
};

// Initialisation du pointeur statique à nullptr
KeyboardControlNode * KeyboardControlNode::instance_ = nullptr;



void sigint_handler(int)
{
    if (KeyboardControlNode::instance_) {
        KeyboardControlNode::instance_->reset_terminal_mode(); // Restaurer terminal
        KeyboardControlNode::instance_ = nullptr;
    }
    std::cout << "\nTerminal restauré. Exit.\n";
    std::exit(0);
}



int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);                   // Initialisation ROS2
    std::signal(SIGINT, sigint_handler);       // Capture CTRL+C pour cleanup propre

    auto node = std::make_shared<KeyboardControlNode>(); // Création du node clavier
    node->run();                                // Lancement de la boucle de lecture clavier

    rclcpp::shutdown();                         // Fermeture propre de ROS2
    return 0;
}
