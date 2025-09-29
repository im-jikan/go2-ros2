#include <rclcpp/rclcpp.hpp>                            // ROS 2 C++ API principale
#include <geometry_msgs/msg/twist.hpp>                  // Message Twist pour vitesses linéaires et angulaires
#include "unitree_api/msg/request.hpp"                   // Message Request spécifique API robot Unitree
#include "ros2_sport_client.h"                           // Client API Sport pour encapsuler commandes robot
#include <chrono>                                        // Pour gestion du temps (timer, durée)

using namespace std::chrono_literals;                   // Facilite écriture durées, ex: 100ms


class MoveRobotNode : public rclcpp::Node
{
public:
    /// Constructeur : initialise publisher, subscriber, timer watchdog, et état interne
    MoveRobotNode() : Node("go2_move"), stop_sent_(false)
    {
        // Publisher sur topic spécifique commandes unitree_api::Request
        req_puber_ = this->create_publisher<unitree_api::msg::Request>("/api/sport/request", 10);

        // Client abstrait permettant d'appeler des méthodes Move, StopMove, Euler
        sport_req_ = std::make_shared<SportClient>();

        // Subscriber pour écouter les commandes de mouvement en Twist
        move_subscriber_ = this->create_subscription<geometry_msgs::msg::Twist>(
            "/go2/move", 10,
            std::bind(&MoveRobotNode::move_callback, this, std::placeholders::_1));

        // Timer périodique pour watchdog, vérifie «timeout» réception commandes
        watchdog_timer_ = this->create_wall_timer(
            100ms,
            std::bind(&MoveRobotNode::watchdog_check, this));

        // Initialisation temps dernière commande reçue à maintenant
        last_command_time_ = this->now();

        RCLCPP_INFO(this->get_logger(), "Abonné au topic /go2/move, en attente de commandes...");
    }

private:
    void move_callback(const geometry_msgs::msg::Twist::SharedPtr msg)
    {
        // Mémorise le temps de réception pour watchdog
        last_command_time_ = this->now();

        // Extraction des composantes linéaires et angulaires
        float vx = msg->linear.x;
        float vy = msg->linear.y;
        float vz = msg->linear.z;

        float roll = msg->angular.x;
        float pitch = msg->angular.y;
        float yaw = msg->angular.z;

        // Création requête commande mouvement ou arrêt
        unitree_api::msg::Request req_move;

        // Si vitesse nulle => commande stop, avec protection anti-spam stop
        if (vx == 0.0f && vy == 0.0f && vz == 0.0f)
        {
            if (!stop_sent_)
            {
                RCLCPP_INFO(this->get_logger(), "Toutes les vitesses à zéro, envoi commande 'stopmove'");
                sport_req_->StopMove(req_move);
                req_puber_->publish(req_move);
                RCLCPP_INFO(this->get_logger(), "Commande 'StopMove' envoyée.");
                stop_sent_ = true; // Flag stop actif
            }
        }
        else // Sinon commande Move avec vitesses reçues
        {
            sport_req_->Move(req_move, vx, vy, vz);
            RCLCPP_INFO(this->get_logger(), "Commande 'Move' => vx: %.2f | vy: %.2f | vz: %.2f", vx, vy, vz);
            req_puber_->publish(req_move);
            stop_sent_ = false; // Commande mouv active, reset flag stop
        }

        // Publication commande Euler (orientation) seulement si non nulle
        if (roll != 0.0f || pitch != 0.0f || yaw != 0.0f)
        {
            unitree_api::msg::Request req_euler;
            RCLCPP_INFO(this->get_logger(), "Commande 'Euler' => Roll: %.2f | Pitch: %.2f | Yaw: %.2f", roll, pitch, yaw);
            sport_req_->Euler(req_euler, roll, pitch, yaw);
            req_puber_->publish(req_euler);
        }
    }

    void watchdog_check()
    {
        if (this->now() - last_command_time_ > rclcpp::Duration(200ms))
        {
            if (!stop_sent_)
            {
                RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 1000,
                    "Pas de commande reçue depuis 200 ms, arrêt du robot.");
                unitree_api::msg::Request req_stop;
                sport_req_->StopMove(req_stop);
                req_puber_->publish(req_stop);
                stop_sent_ = true;
            }
        }
    }

    // Publisher pour envoyer commandes unitree_api::Request au robot
    rclcpp::Publisher<unitree_api::msg::Request>::SharedPtr req_puber_;

    // Subscriber pour recevoir commandes Twist depuis d'autres nodes
    rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr move_subscriber_;

    // Client abstrait encapsulant les commandes robot sport
    std::shared_ptr<SportClient> sport_req_;

    // Timer watchdog périodique
    rclcpp::TimerBase::SharedPtr watchdog_timer_;

    // Horodatage de la dernière commande effective reçue
    rclcpp::Time last_command_time_;

    // Indicateur si dernière commande était un arrêt (pour éviter spam)
    bool stop_sent_;
};


int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);                // Démarrage communication ROS 2
    auto node = std::make_shared<MoveRobotNode>();  // Création du node
    rclcpp::spin(node);                      // Traitement callbacks / événements ROS
    rclcpp::shutdown();                      // Arrêt propre
    return 0;
}
