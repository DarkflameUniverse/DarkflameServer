#pragma once

// Property model import and removal
void RegisterMaintenanceRoutes();

// Scheduled tasks (approving pet names that were approved before, filling in pet owners); before Scheduler::Initialize
void RegisterMaintenanceTasks();
