#pragma once

/**
 * Saved Economy page views (per account) and the report_emails task that emails them daily or weekly.
 * Views keep a relative range (last 7/30/90/365 days), so an emailed report always covers the latest days.
 */
void RegisterReportViewRoutes();

// Before Scheduler::Initialize
void RegisterReportViewTask();
