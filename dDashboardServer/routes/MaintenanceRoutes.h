#pragma once

// Operator data repairs (maintenance page) and model import
void RegisterMaintenanceRoutes();

// Scheduled tasks (approving pet names that were approved before, filling in pet owners); before Scheduler::Initialize
void RegisterMaintenanceTasks();
