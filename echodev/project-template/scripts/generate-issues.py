#!/usr/bin/env python3
"""
Unreal-Echo Issue Generator

Generates GitHub issues from roadmap JSON data for tracking development
progress across multiple organizations.

Usage:
    python generate-issues.py --roadmap roadmap.json --org orgitcog --repo u9n
    python generate-issues.py --roadmap roadmap.json --dry-run
    python generate-issues.py --roadmap roadmap.json --epic E1 --org orgitcog --repo u9n
    python generate-issues.py --roadmap roadmap.json --deduplicate
    python generate-issues.py --roadmap roadmap.json --deduplicate --dry-run
"""

import argparse
import json
import os
import subprocess
import sys
from dataclasses import dataclass
from typing import List, Dict, Optional, Set
from datetime import datetime


@dataclass
class Feature:
    id: str
    title: str
    description: str
    priority: str = "medium"
    effort_points: int = 3
    labels: List[str] = None
    acceptance_criteria: List[str] = None
    technical_notes: str = ""
    
    def to_issue_body(self, phase_id: str, epic_id: str) -> str:
        body = f"""## Feature: {self.id}

**Parent Phase:** {phase_id}
**Parent Epic:** {epic_id}
**Priority:** {self.priority.capitalize()}
**Effort:** {self.effort_points} points

### Description

{self.description}

### Acceptance Criteria

"""
        if self.acceptance_criteria:
            for criterion in self.acceptance_criteria:
                body += f"- [ ] {criterion}\n"
        else:
            body += "- [ ] Feature implemented\n- [ ] Tests passing\n- [ ] Documentation updated\n"
        
        if self.technical_notes:
            body += f"""
### Technical Notes

{self.technical_notes}
"""
        
        body += f"""
---
*Generated from Unreal-Echo Development Roadmap*
*Feature ID: {self.id} | Phase: {phase_id} | Epic: {epic_id}*
"""
        return body


@dataclass
class Phase:
    id: str
    title: str
    description: str = ""
    duration_weeks: int = 4
    features: List[Feature] = None
    
    def to_issue_body(self, epic_id: str) -> str:
        body = f"""## Phase: {self.id}

**Parent Epic:** {epic_id}
**Duration:** {self.duration_weeks} weeks

### Description

{self.description}

### Features

"""
        if self.features:
            for feature in self.features:
                body += f"- [ ] {feature.id}: {feature.title}\n"
        
        body += f"""
### Deliverables

- [ ] All features implemented
- [ ] Unit tests passing
- [ ] Integration tests passing
- [ ] Documentation updated

---
*Generated from Unreal-Echo Development Roadmap*
*Phase ID: {self.id} | Epic: {epic_id}*
"""
        return body


@dataclass
class Epic:
    id: str
    title: str
    description: str = ""
    duration_months: int = 6
    priority: str = "high"
    phases: List[Phase] = None
    dependencies: List[str] = None
    labels: List[str] = None
    
    def to_issue_body(self) -> str:
        body = f"""## Epic: {self.id}

**Duration:** {self.duration_months} months
**Priority:** {self.priority.capitalize()}

### Description

{self.description}

### Phases

"""
        if self.phases:
            for phase in self.phases:
                body += f"- [ ] Phase {phase.id}: {phase.title}\n"
        
        if self.dependencies:
            body += "\n### Dependencies\n\n"
            for dep in self.dependencies:
                body += f"- {dep}\n"
        
        body += f"""
### Success Criteria

- [ ] All phases completed
- [ ] Integration tests passing
- [ ] Performance benchmarks met
- [ ] Documentation complete

---
*Generated from Unreal-Echo Development Roadmap*
*Epic ID: {self.id}*
"""
        return body


class IssueGenerator:
    def __init__(self, org: str, repo: str, dry_run: bool = False):
        self.org = org
        self.repo = repo
        self.dry_run = dry_run
        self.created_issues: Dict[str, int] = {}
        self._existing_titles: Optional[Dict[str, int]] = None

    def _load_existing_issues(self) -> Dict[str, int]:
        """Fetch all open issue titles from the repo (title -> lowest issue number)."""
        if self._existing_titles is not None:
            return self._existing_titles
        print(f"  Fetching existing open issues from {self.org}/{self.repo}...")
        cmd = [
            "gh", "issue", "list",
            "--repo", f"{self.org}/{self.repo}",
            "--state", "open",
            "--limit", "2000",
            "--json", "number,title",
        ]
        try:
            result = subprocess.run(cmd, capture_output=True, text=True, check=True)
            items = json.loads(result.stdout)
            # Map title -> lowest issue number (keep the original)
            mapping: Dict[str, int] = {}
            for item in sorted(items, key=lambda x: x["number"]):
                title = item["title"]
                if title not in mapping:
                    mapping[title] = item["number"]
            self._existing_titles = mapping
            print(f"  Found {len(mapping)} existing open issues.")
        except (subprocess.CalledProcessError, json.JSONDecodeError) as e:
            print(f"  Warning: could not fetch existing issues: {e}")
            self._existing_titles = {}
        return self._existing_titles

    def issue_exists(self, title: str) -> Optional[int]:
        """Return the existing issue number if an open issue with this title exists, else None."""
        existing = self._load_existing_issues()
        return existing.get(title)

    def deduplicate_issues(self) -> None:
        """Close all duplicate open issues, keeping the lowest-numbered original."""
        print(f"\nFetching all open issues from {self.org}/{self.repo} for deduplication...")
        cmd = [
            "gh", "issue", "list",
            "--repo", f"{self.org}/{self.repo}",
            "--state", "open",
            "--limit", "2000",
            "--json", "number,title",
        ]
        try:
            result = subprocess.run(cmd, capture_output=True, text=True, check=True)
            items = json.loads(result.stdout)
        except (subprocess.CalledProcessError, json.JSONDecodeError) as e:
            print(f"  Error fetching issues: {e}")
            return

        # Group by title, sorted by number ascending
        title_map: Dict[str, List[int]] = {}
        for item in sorted(items, key=lambda x: x["number"]):
            title_map.setdefault(item["title"], []).append(item["number"])

        duplicates = {t: nums for t, nums in title_map.items() if len(nums) > 1}
        if not duplicates:
            print("  No duplicate open issues found.")
            return

        total_to_close = sum(len(v) - 1 for v in duplicates.values())
        print(f"  Found {len(duplicates)} duplicate groups, {total_to_close} issues to close.")

        closed = 0
        skipped = 0
        for title, nums in sorted(duplicates.items()):
            keep = nums[0]
            for dup_num in nums[1:]:
                if self.dry_run:
                    print(f"  [DRY RUN] Would close #{dup_num} (duplicate of #{keep}): {title}")
                    skipped += 1
                else:
                    close_cmd = [
                        "gh", "issue", "close", str(dup_num),
                        "--repo", f"{self.org}/{self.repo}",
                    ]
                    try:
                        subprocess.run(close_cmd, capture_output=True, text=True, check=True)
                        print(f"  Closed #{dup_num} (duplicate of #{keep}): {title}")
                        closed += 1
                    except subprocess.CalledProcessError as e:
                        print(f"  Error closing #{dup_num}: {e.stderr.strip()}")
                        skipped += 1

        print(f"\nDeduplication complete: {closed} closed, {skipped} skipped.")

    def create_issue(self, title: str, body: str, labels: List[str],
                     milestone: Optional[str] = None) -> Optional[int]:
        """Create a GitHub issue using gh CLI, skipping if one already exists."""
        existing_num = self.issue_exists(title)
        if existing_num is not None:
            print(f"  Skipping (already exists as #{existing_num}): {title}")
            return existing_num

        if self.dry_run:
            print(f"\n[DRY RUN] Would create issue:")
            print(f"  Title: {title}")
            print(f"  Labels: {', '.join(labels)}")
            print(f"  Milestone: {milestone}")
            print(f"  Body preview: {body[:200]}...")
            return None

        cmd = [
            "gh", "issue", "create",
            "--repo", f"{self.org}/{self.repo}",
            "--title", title,
            "--body", body
        ]

        for label in labels:
            cmd.extend(["--label", label])

        if milestone:
            cmd.extend(["--milestone", milestone])

        try:
            result = subprocess.run(cmd, capture_output=True, text=True, check=True)
            # Extract issue number from URL
            issue_url = result.stdout.strip()
            issue_num = int(issue_url.split("/")[-1])
            print(f"  Created: {issue_url}")
            # Register in cache to prevent duplicates within this run
            self._existing_titles[title] = issue_num
            return issue_num
        except subprocess.CalledProcessError as e:
            print(f"  Error creating issue: {e.stderr}")
            return None
    
    def create_labels(self, labels_config: Dict) -> None:
        """Create labels in the repository."""
        print(f"\nCreating labels in {self.org}/{self.repo}...")
        
        for category, label_list in labels_config.items():
            for label in label_list:
                if self.dry_run:
                    print(f"  [DRY RUN] Would create label: {label['name']}")
                    continue
                
                cmd = [
                    "gh", "label", "create",
                    label["name"],
                    "--repo", f"{self.org}/{self.repo}",
                    "--color", label["color"],
                    "--description", label.get("description", ""),
                    "--force"
                ]
                
                try:
                    subprocess.run(cmd, capture_output=True, check=True)
                    print(f"  Created label: {label['name']}")
                except subprocess.CalledProcessError:
                    pass  # Label may already exist
    
    def create_milestones(self, milestones: List[Dict]) -> None:
        """Create milestones in the repository."""
        print(f"\nCreating milestones in {self.org}/{self.repo}...")
        
        for ms in milestones:
            if self.dry_run:
                print(f"  [DRY RUN] Would create milestone: {ms['title']}")
                continue
            
            cmd = [
                "gh", "api",
                f"/repos/{self.org}/{self.repo}/milestones",
                "-X", "POST",
                "-f", f"title={ms['title']}",
                "-f", f"description={ms['description']}"
            ]
            
            if "target_date" in ms:
                cmd.extend(["-f", f"due_on={ms['target_date']}T23:59:59Z"])
            
            try:
                subprocess.run(cmd, capture_output=True, check=True)
                print(f"  Created milestone: {ms['title']}")
            except subprocess.CalledProcessError:
                pass  # Milestone may already exist
    
    def generate_epic_issues(self, epic: Epic, milestone: Optional[str] = None) -> None:
        """Generate issues for an epic and all its phases and features."""
        print(f"\n{'='*60}")
        print(f"Generating issues for Epic {epic.id}: {epic.title}")
        print(f"{'='*60}")
        
        # Create epic issue
        labels = ["epic", "roadmap"]
        if epic.labels:
            labels.extend(epic.labels)
        labels.append(f"E{epic.id[1:]}: {epic.title.split(':')[0] if ':' in epic.title else epic.title[:20]}")
        
        epic_issue_num = self.create_issue(
            title=f"[EPIC {epic.id}] {epic.title}",
            body=epic.to_issue_body(),
            labels=labels,
            milestone=milestone
        )
        
        if epic_issue_num:
            self.created_issues[epic.id] = epic_issue_num
        
        # Create phase issues
        if epic.phases:
            for phase in epic.phases:
                self.generate_phase_issues(phase, epic.id, milestone)
    
    def generate_phase_issues(self, phase: Phase, epic_id: str, 
                              milestone: Optional[str] = None) -> None:
        """Generate issues for a phase and all its features."""
        print(f"\n  Phase {phase.id}: {phase.title}")
        
        labels = ["phase", "roadmap"]
        
        phase_issue_num = self.create_issue(
            title=f"[PHASE {phase.id}] {phase.title}",
            body=phase.to_issue_body(epic_id),
            labels=labels,
            milestone=milestone
        )
        
        if phase_issue_num:
            self.created_issues[phase.id] = phase_issue_num
        
        # Create feature issues
        if phase.features:
            for feature in phase.features:
                self.generate_feature_issue(feature, phase.id, epic_id, milestone)
    
    def generate_feature_issue(self, feature: Feature, phase_id: str, 
                               epic_id: str, milestone: Optional[str] = None) -> None:
        """Generate issue for a single feature."""
        print(f"    Feature {feature.id}: {feature.title}")
        
        labels = ["feature", "roadmap"]
        if feature.labels:
            labels.extend(feature.labels)
        labels.append(f"priority: {feature.priority}")
        labels.append(f"effort: {feature.effort_points}")
        
        feature_issue_num = self.create_issue(
            title=f"[FEATURE {feature.id}] {feature.title}",
            body=feature.to_issue_body(phase_id, epic_id),
            labels=labels,
            milestone=milestone
        )
        
        if feature_issue_num:
            self.created_issues[feature.id] = feature_issue_num


def load_roadmap(filepath: str) -> Dict:
    """Load roadmap from JSON file."""
    with open(filepath, 'r') as f:
        return json.load(f)


def parse_epic(epic_data: Dict) -> Epic:
    """Parse epic data into Epic object."""
    phases = []
    if "phases" in epic_data:
        for phase_data in epic_data["phases"]:
            features = []
            if "features" in phase_data:
                for feat_data in phase_data["features"]:
                    features.append(Feature(
                        id=feat_data.get("id", ""),
                        title=feat_data.get("title", ""),
                        description=feat_data.get("description", ""),
                        priority=feat_data.get("priority", "medium"),
                        effort_points=feat_data.get("effort_points", 3),
                        labels=feat_data.get("labels", []),
                        acceptance_criteria=feat_data.get("acceptance_criteria", []),
                        technical_notes=feat_data.get("technical_notes", "")
                    ))
            
            phases.append(Phase(
                id=phase_data.get("id", ""),
                title=phase_data.get("title", ""),
                description=phase_data.get("description", ""),
                duration_weeks=phase_data.get("duration_weeks", 4),
                features=features
            ))
    
    return Epic(
        id=epic_data.get("id", ""),
        title=epic_data.get("title", ""),
        description=epic_data.get("description", ""),
        duration_months=epic_data.get("duration_months", 6),
        priority=epic_data.get("priority", "high"),
        phases=phases,
        dependencies=epic_data.get("dependencies", []),
        labels=epic_data.get("labels", [])
    )


def main():
    parser = argparse.ArgumentParser(
        description="Generate GitHub issues from Unreal-Echo roadmap"
    )
    parser.add_argument(
        "--roadmap", "-r",
        required=True,
        help="Path to roadmap JSON file"
    )
    parser.add_argument(
        "--org", "-o",
        default="orgitcog",
        help="GitHub organization (default: orgitcog)"
    )
    parser.add_argument(
        "--repo",
        default="u9n",
        help="GitHub repository (default: u9n)"
    )
    parser.add_argument(
        "--epic", "-e",
        help="Generate issues for specific epic only (e.g., E1)"
    )
    parser.add_argument(
        "--dry-run", "-n",
        action="store_true",
        help="Show what would be created/closed without actually doing it"
    )
    parser.add_argument(
        "--deduplicate",
        action="store_true",
        help="Close duplicate open issues, keeping the lowest-numbered original"
    )
    parser.add_argument(
        "--create-labels",
        action="store_true",
        help="Create labels from config"
    )
    parser.add_argument(
        "--create-milestones",
        action="store_true",
        help="Create milestones from config"
    )
    parser.add_argument(
        "--labels-config",
        help="Path to labels JSON config"
    )
    parser.add_argument(
        "--milestones-config",
        help="Path to milestones JSON config"
    )
    
    args = parser.parse_args()
    
    # Load roadmap
    print(f"Loading roadmap from {args.roadmap}...")
    roadmap = load_roadmap(args.roadmap)
    
    # Initialize generator
    generator = IssueGenerator(args.org, args.repo, args.dry_run)

    # Deduplicate existing issues if requested
    if args.deduplicate:
        generator.deduplicate_issues()
        return

    # Create labels if requested
    if args.create_labels and args.labels_config:
        labels_config = load_roadmap(args.labels_config)
        generator.create_labels(labels_config.get("labels", {}))
    
    # Create milestones if requested
    if args.create_milestones and args.milestones_config:
        milestones_config = load_roadmap(args.milestones_config)
        generator.create_milestones(milestones_config.get("milestones", []))
    
    # Generate issues
    epics = roadmap.get("epics", [])
    
    for epic_data in epics:
        epic = parse_epic(epic_data)
        
        # Filter by epic if specified
        if args.epic and epic.id != args.epic:
            continue
        
        generator.generate_epic_issues(epic)
    
    # Summary
    print(f"\n{'='*60}")
    print("Summary")
    print(f"{'='*60}")
    print(f"Total issues created: {len(generator.created_issues)}")
    
    if generator.created_issues:
        print("\nCreated issues:")
        for issue_id, issue_num in generator.created_issues.items():
            print(f"  {issue_id}: #{issue_num}")


if __name__ == "__main__":
    main()
