#!/usr/bin/env python3
"""
Cross-Organization Issue Synchronization

Synchronizes issues and progress tracking across multiple GitHub organizations
participating in the Unreal-Echo development.

Usage:
    python sync-orgs.py --config orgs.json --action sync
    python sync-orgs.py --config orgs.json --action report
    python sync-orgs.py --config orgs.json --action create-tracking-issue
"""

import argparse
import json
import subprocess
import sys
from dataclasses import dataclass
from typing import List, Dict, Optional
from datetime import datetime


@dataclass
class Organization:
    name: str
    github_org: str
    role: str
    repos: List[str]
    tracking_repo: Optional[str] = None


@dataclass
class IssueStatus:
    org: str
    repo: str
    issue_number: int
    title: str
    state: str
    labels: List[str]
    assignees: List[str]
    milestone: Optional[str]
    created_at: str
    updated_at: str
    feature_id: Optional[str] = None


class CrossOrgSync:
    def __init__(self, config_path: str, dry_run: bool = False):
        self.config = self._load_config(config_path)
        self.dry_run = dry_run
        self.orgs: List[Organization] = []
        self._parse_orgs()
        
    def _load_config(self, path: str) -> Dict:
        with open(path, 'r') as f:
            return json.load(f)
    
    def _parse_orgs(self):
        for org_data in self.config.get("organizations", []):
            self.orgs.append(Organization(
                name=org_data["name"],
                github_org=org_data["github_org"],
                role=org_data["role"],
                repos=org_data.get("repos", []),
                tracking_repo=org_data.get("tracking_repo")
            ))
    
    def _run_gh_command(self, cmd: List[str]) -> Optional[str]:
        """Run a gh CLI command and return output."""
        try:
            result = subprocess.run(cmd, capture_output=True, text=True, check=True)
            return result.stdout.strip()
        except subprocess.CalledProcessError as e:
            print(f"Error running command: {' '.join(cmd)}")
            print(f"Error: {e.stderr}")
            return None
    
    def get_issues_by_label(self, org: str, repo: str, label: str) -> List[Dict]:
        """Get all issues with a specific label."""
        cmd = [
            "gh", "issue", "list",
            "--repo", f"{org}/{repo}",
            "--label", label,
            "--json", "number,title,state,labels,assignees,milestone,createdAt,updatedAt",
            "--limit", "1000"
        ]
        
        output = self._run_gh_command(cmd)
        if output:
            return json.loads(output)
        return []
    
    def get_all_roadmap_issues(self) -> Dict[str, List[IssueStatus]]:
        """Get all roadmap issues across all organizations."""
        all_issues: Dict[str, List[IssueStatus]] = {}
        
        for org in self.orgs:
            org_key = org.github_org
            all_issues[org_key] = []
            
            for repo in org.repos:
                print(f"Fetching issues from {org_key}/{repo}...")
                issues = self.get_issues_by_label(org_key, repo, "roadmap")
                
                for issue in issues:
                    # Extract feature ID from title if present
                    feature_id = None
                    title = issue.get("title", "")
                    if "[FEATURE " in title:
                        start = title.find("[FEATURE ") + 9
                        end = title.find("]", start)
                        if end > start:
                            feature_id = title[start:end]
                    elif "[PHASE " in title:
                        start = title.find("[PHASE ") + 7
                        end = title.find("]", start)
                        if end > start:
                            feature_id = f"P{title[start:end]}"
                    elif "[EPIC " in title:
                        start = title.find("[EPIC ") + 6
                        end = title.find("]", start)
                        if end > start:
                            feature_id = title[start:end]
                    
                    all_issues[org_key].append(IssueStatus(
                        org=org_key,
                        repo=repo,
                        issue_number=issue.get("number", 0),
                        title=title,
                        state=issue.get("state", "open"),
                        labels=[l.get("name", "") for l in issue.get("labels", [])],
                        assignees=[a.get("login", "") for a in issue.get("assignees", [])],
                        milestone=issue.get("milestone", {}).get("title") if issue.get("milestone") else None,
                        created_at=issue.get("createdAt", ""),
                        updated_at=issue.get("updatedAt", ""),
                        feature_id=feature_id
                    ))
        
        return all_issues
    
    def generate_progress_report(self) -> str:
        """Generate a markdown progress report across all organizations."""
        all_issues = self.get_all_roadmap_issues()
        
        report = f"""# Unreal-Echo Cross-Organization Progress Report

**Generated:** {datetime.now().strftime("%Y-%m-%d %H:%M:%S UTC")}

## Summary

"""
        
        total_issues = 0
        total_open = 0
        total_closed = 0
        
        for org_key, issues in all_issues.items():
            org_open = sum(1 for i in issues if i.state == "OPEN")
            org_closed = sum(1 for i in issues if i.state == "CLOSED")
            total_issues += len(issues)
            total_open += org_open
            total_closed += org_closed
            
            report += f"- **{org_key}**: {len(issues)} issues ({org_open} open, {org_closed} closed)\n"
        
        report += f"""
**Total:** {total_issues} issues ({total_open} open, {total_closed} closed)
**Completion:** {(total_closed / total_issues * 100) if total_issues > 0 else 0:.1f}%

## Progress by Epic

"""
        
        # Group by epic
        epics: Dict[str, List[IssueStatus]] = {}
        for org_key, issues in all_issues.items():
            for issue in issues:
                if issue.feature_id:
                    if issue.feature_id.startswith("E"):
                        epic_id = issue.feature_id
                    elif issue.feature_id.startswith("F"):
                        epic_id = f"E{issue.feature_id[1]}"
                    elif issue.feature_id.startswith("P"):
                        epic_id = f"E{issue.feature_id[1]}"
                    else:
                        epic_id = "Unknown"
                    
                    if epic_id not in epics:
                        epics[epic_id] = []
                    epics[epic_id].append(issue)
        
        for epic_id in sorted(epics.keys()):
            epic_issues = epics[epic_id]
            epic_open = sum(1 for i in epic_issues if i.state == "OPEN")
            epic_closed = sum(1 for i in epic_issues if i.state == "CLOSED")
            epic_total = len(epic_issues)
            completion = (epic_closed / epic_total * 100) if epic_total > 0 else 0
            
            report += f"""### {epic_id}

- Total Issues: {epic_total}
- Open: {epic_open}
- Closed: {epic_closed}
- Completion: {completion:.1f}%

| Feature ID | Title | Status | Org/Repo | Assignees |
|------------|-------|--------|----------|-----------|
"""
            
            for issue in sorted(epic_issues, key=lambda x: x.feature_id or ""):
                assignees = ", ".join(issue.assignees) if issue.assignees else "-"
                status_emoji = "✅" if issue.state == "CLOSED" else "🔄"
                report += f"| {issue.feature_id or '-'} | {issue.title[:50]}... | {status_emoji} {issue.state} | {issue.org}/{issue.repo} | {assignees} |\n"
            
            report += "\n"
        
        report += """## Organization Details

"""
        
        for org in self.orgs:
            org_issues = all_issues.get(org.github_org, [])
            report += f"""### {org.name} ({org.github_org})

**Role:** {org.role.capitalize()}
**Repositories:** {', '.join(org.repos)}
**Issues:** {len(org_issues)}

"""
        
        return report
    
    def create_tracking_issue(self, target_org: str, target_repo: str) -> Optional[int]:
        """Create a master tracking issue in the specified repository."""
        report = self.generate_progress_report()
        
        title = f"[TRACKING] Unreal-Echo Cross-Org Progress - {datetime.now().strftime('%Y-%m-%d')}"
        
        body = f"""## Cross-Organization Progress Tracking

This issue tracks progress across all organizations participating in Unreal-Echo development.

{report}

---
*This tracking issue is automatically generated and updated.*
*Last updated: {datetime.now().strftime("%Y-%m-%d %H:%M:%S UTC")}*
"""
        
        if self.dry_run:
            print(f"\n[DRY RUN] Would create tracking issue in {target_org}/{target_repo}")
            print(f"Title: {title}")
            print(f"Body preview: {body[:500]}...")
            return None
        
        cmd = [
            "gh", "issue", "create",
            "--repo", f"{target_org}/{target_repo}",
            "--title", title,
            "--body", body,
            "--label", "tracking,roadmap"
        ]
        
        output = self._run_gh_command(cmd)
        if output:
            issue_num = int(output.split("/")[-1])
            print(f"Created tracking issue: {output}")
            return issue_num
        return None
    
    def sync_labels(self, source_org: str, source_repo: str, 
                    target_org: str, target_repo: str) -> None:
        """Sync labels from source to target repository."""
        print(f"Syncing labels from {source_org}/{source_repo} to {target_org}/{target_repo}...")
        
        # Get labels from source
        cmd = [
            "gh", "label", "list",
            "--repo", f"{source_org}/{source_repo}",
            "--json", "name,color,description"
        ]
        
        output = self._run_gh_command(cmd)
        if not output:
            return
        
        labels = json.loads(output)
        
        for label in labels:
            if self.dry_run:
                print(f"  [DRY RUN] Would create label: {label['name']}")
                continue
            
            cmd = [
                "gh", "label", "create",
                label["name"],
                "--repo", f"{target_org}/{target_repo}",
                "--color", label["color"],
                "--description", label.get("description", ""),
                "--force"
            ]
            
            self._run_gh_command(cmd)
            print(f"  Synced label: {label['name']}")
    
    def sync_milestones(self, source_org: str, source_repo: str,
                        target_org: str, target_repo: str) -> None:
        """Sync milestones from source to target repository."""
        print(f"Syncing milestones from {source_org}/{source_repo} to {target_org}/{target_repo}...")
        
        # Get milestones from source
        cmd = [
            "gh", "api",
            f"/repos/{source_org}/{source_repo}/milestones",
            "--jq", ".[].title"
        ]
        
        output = self._run_gh_command(cmd)
        if not output:
            return
        
        for title in output.split("\n"):
            if not title:
                continue
            
            if self.dry_run:
                print(f"  [DRY RUN] Would create milestone: {title}")
                continue
            
            cmd = [
                "gh", "api",
                f"/repos/{target_org}/{target_repo}/milestones",
                "-X", "POST",
                "-f", f"title={title}"
            ]
            
            self._run_gh_command(cmd)
            print(f"  Synced milestone: {title}")


def main():
    parser = argparse.ArgumentParser(
        description="Cross-organization issue synchronization for Unreal-Echo"
    )
    parser.add_argument(
        "--config", "-c",
        required=True,
        help="Path to organizations config JSON"
    )
    parser.add_argument(
        "--action", "-a",
        choices=["sync", "report", "create-tracking-issue", "sync-labels", "sync-milestones"],
        required=True,
        help="Action to perform"
    )
    parser.add_argument(
        "--target-org",
        help="Target organization for tracking issue or sync"
    )
    parser.add_argument(
        "--target-repo",
        help="Target repository for tracking issue or sync"
    )
    parser.add_argument(
        "--source-org",
        help="Source organization for sync operations"
    )
    parser.add_argument(
        "--source-repo",
        help="Source repository for sync operations"
    )
    parser.add_argument(
        "--output", "-o",
        help="Output file for report"
    )
    parser.add_argument(
        "--dry-run", "-n",
        action="store_true",
        help="Show what would be done without making changes"
    )
    
    args = parser.parse_args()
    
    sync = CrossOrgSync(args.config, args.dry_run)
    
    if args.action == "report":
        report = sync.generate_progress_report()
        if args.output:
            with open(args.output, 'w') as f:
                f.write(report)
            print(f"Report written to {args.output}")
        else:
            print(report)
    
    elif args.action == "create-tracking-issue":
        if not args.target_org or not args.target_repo:
            print("Error: --target-org and --target-repo required for create-tracking-issue")
            sys.exit(1)
        sync.create_tracking_issue(args.target_org, args.target_repo)
    
    elif args.action == "sync-labels":
        if not all([args.source_org, args.source_repo, args.target_org, args.target_repo]):
            print("Error: --source-org, --source-repo, --target-org, --target-repo required")
            sys.exit(1)
        sync.sync_labels(args.source_org, args.source_repo, args.target_org, args.target_repo)
    
    elif args.action == "sync-milestones":
        if not all([args.source_org, args.source_repo, args.target_org, args.target_repo]):
            print("Error: --source-org, --source-repo, --target-org, --target-repo required")
            sys.exit(1)
        sync.sync_milestones(args.source_org, args.source_repo, args.target_org, args.target_repo)
    
    elif args.action == "sync":
        print("Full sync not yet implemented. Use individual sync commands.")


if __name__ == "__main__":
    main()
