import pychrono.core as chrono
import pychrono.vehicle as veh
import pychrono.irrlicht as irr
import math
import csv
import os

system = chrono.ChSystemSMC()
system.SetGravitationalAcceleration(chrono.ChVector3d(0, -9.81, 0))
system.SetCollisionSystemType(chrono.ChCollisionSystem.Type_BULLET)


terrain = veh.SCMTerrain(system)
terrain.SetSoilParameters(
    0.2e6, 0, 1.1, 0, 30, 0.01, 4e7, 3e4
)
terrain.Initialize(20.0, 20.0, 0.05)

wheel_radius = 0.125  
target_speed_kmh = 8.0
target_speed_ms = target_speed_kmh / 3.6
target_omega = target_speed_ms / wheel_radius 


total_test_mass = 260.0 


mesh_path = "wheel with 45 degree thread pattern.obj"
if not os.path.exists(mesh_path):
    print(f"\n[ERROR] Could not find: '{mesh_path}'")
    exit()

mat = chrono.ChContactMaterialSMC()
mat.SetFriction(0.9)
mat.SetYoungModulus(1e7)

wheel_body = chrono.ChBodyEasyMesh(
    mesh_path, 
    1000.0,  
    True, True, True, mat      
)
wheel_body.SetMass(5.0) 
wheel_body.SetInertiaXX(chrono.ChVector3d(0.1, 0.1, 0.1))
wheel_body.SetPos(chrono.ChVector3d(0, wheel_radius + 0.05, 0))
system.Add(wheel_body)


ground = chrono.ChBody()
ground.SetFixed(True)
system.Add(ground)

carrier = chrono.ChBody()
carrier.SetMass(total_test_mass - 5.0) 
carrier.SetPos(wheel_body.GetPos())
system.Add(carrier)

align_link = chrono.ChLinkLockAlign()
align_link.Initialize(carrier, ground, chrono.ChFramed(carrier.GetPos()))
system.AddLink(align_link)

motor = chrono.ChLinkMotorRotationSpeed()
motor.Initialize(wheel_body, carrier, chrono.ChFramed(wheel_body.GetPos(), chrono.QuatFromAngleY(math.pi/2)))
motor.SetSpeedFunction(chrono.ChFunctionConst(target_omega))
system.AddLink(motor)


vis = irr.ChVisualSystemIrrlicht()
vis.AttachSystem(system)
vis.SetWindowSize(1280, 720)
vis.SetWindowTitle("Project Chrono By NASA")
vis.Initialize()
vis.AddTypicalLights()
vis.AddSkyBox()
vis.AddCamera(chrono.ChVector3d(2.0, 1.0, -2.5), chrono.ChVector3d(0, 0, 0))


output_csv = "sim_output_results.csv"
print(f"\nSTARTING Simulating Project Chrono 2.5kN load. Data logging to {output_csv}...")

with open(output_csv, mode='w', newline='') as file:
    writer = csv.writer(file)
    writer.writerow(["Time (s)", "Sinkage (m)", "Tractive Effort (N)", "Actual Velocity (m/s)"])

    step_size = 1e-3
    while vis.Run():
        vis.BeginScene()
        vis.Render()
        vis.EndScene()
        
        time = system.GetChTime()
        vel = wheel_body.GetPosDt().x        
        motor_torque = abs(motor.GetMotorTorque())
        tractive_effort = motor_torque / wheel_radius
        
        sinkage = 0.05 - (wheel_body.GetPos().y - wheel_radius)

        if round(time, 3) % 0.05 == 0:
            writer.writerow([time, sinkage, tractive_effort, vel])

        system.DoStepDynamics(step_size)