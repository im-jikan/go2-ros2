from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
import os

def generate_launch_description():

    # === Chemins des fichiers partagés ===
    go2_bringup_share = get_package_share_directory('go2_bringup')

    return LaunchDescription([

        # === Joystick ===
        Node(
            package='joy',
            executable='joy_node',
            name='joy_node',
            output='screen'
        ),

        # === Contrôle du robot ===
        Node(
            package='go2_control',
            executable='teleop',
            name='teleop',
            output='screen'
        ),

        Node(
            package='go2_control',
            executable='move',
            name='move',
            output='screen'
        )
    ])
