#ifndef CHECKER_H
#define CHECKER_H

#include "sysinfo.h"
#include "req_parser.h"

typedef enum {
    STATUS_PASS,
    STATUS_WARN,
    STATUS_FAIL,
    STATUS_UNKNOWN
} CheckStatus;

typedef struct {
    CheckStatus status;
    char req_text[256];
    char sys_text[256];
    char detail[256];
} ComponentCheck;

typedef enum {
    TIER_MEETS_RECOMMENDED,
    TIER_MEETS_MINIMUM,
    TIER_BORDERLINE,
    TIER_BELOW_MINIMUM,
    TIER_NO_REQUIREMENTS
} OverallVerdict;

typedef struct {
    ComponentCheck ram;
    ComponentCheck storage;
    ComponentCheck cpu;
    ComponentCheck gpu;
    ComponentCheck os;

    OverallVerdict verdict;
    char verdict_headline[128];
    char verdict_details[512];
    char recommendations[512];
} EvaluationReport;

void evaluate_compatibility(const SystemSpecs *specs, const GameRequirements *game,
                            EvaluationReport *min_rep, EvaluationReport *rec_rep);

void print_compatibility_report(const SystemSpecs *specs, const GameRequirements *game,
                                const EvaluationReport *min_rep, const EvaluationReport *rec_rep);

void print_json_report(const SystemSpecs *specs, const GameRequirements *game,
                       const EvaluationReport *min_rep, const EvaluationReport *rec_rep);

#endif /* CHECKER_H */
